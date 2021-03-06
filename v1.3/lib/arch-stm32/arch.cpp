#include <monty.h>
#include "arch.h"

#include <cassert>
#include <unistd.h>

#include <jee/text-ihex.h>

using namespace monty;

uint8_t arch::Device::irqMap [];
arch::Device* arch::Device::devMap [];

#if HAS_MRFS
const auto mrfsBase = (mrfs::Info*) 0x08010000;
const auto mrfsSize = 32*1024;
#endif

#if STM32L432xx
UartBufDev< PinA<2>, PinA<15>, 100 > console;
#elif STM32F413xx || NUCLEO_H743ZI
UartBufDev< PinD<8>, PinD<9>, 100 > console;
#elif STM32F469xx
UartBufDev< PinB<10>, PinB<11>, 100 > console;
#elif STM32L496xx
UartBufDev< PinA<2>, PinD<6>, 100 > console;
#elif STM32F4 || STM32L412xx || NUCLEO_L073RZ || NUCLEO_L476RG
UartBufDev< PinA<2>, PinA<3>, 100 > console;
#elif STM32F723xx
UartBufDev< PinC<6>, PinC<7>, 100 > console;
#elif DISCO_F746NG || STM32F750xx
UartBufDev< PinA<9>, PinB<7>, 100 > console;
#else
UartBufDev< PinA<9>, PinA<10>, 100 > console;
#endif

// print a string on the polled uart, can be used with interrupts disabled
// ... but the elapsed time waiting for the uart will slow things down a lot
void kputs (char const* msg) {
    auto& polled = (decltype(console)::base&) console;
    while (*msg)
        polled.putc(*msg++);
}

// give up, but not before trying to send a final message to the console port
void panic (char const* msg) {
    for (int i = 0; i < 10000000; ++i) asm (""); // give uart time to settle
    asm volatile ("cpsid if"); // disable interrupts and faults
    kputs("\n*** panic: "); kputs(msg); kputs(" ***\n");
    while (true) {} // hang
}

// set up and enable the main fault handlers
extern "C" void HardFault_Handler  () { panic("hard fault"); };
extern "C" void MemManage_Handler  () { panic("mem fault"); };
extern "C" void BusFault_Handler   () { panic("bus fault"); };
extern "C" void UsageFault_Handler () { panic("usage fault"); };

void setupFaultHandlers () {
    MMIO32(0xE000ED24) |= 0b111<<16; // SCB->SHCSR |= (USG|BUS|MEM)FAULTENA
}

static void outch (int c) {
    console.putc(c);
}

static auto outFun = outch;

int printf(char const* fmt, ...) {
    va_list ap; va_start(ap, fmt); veprintf(outFun, fmt, ap); va_end(ap);
    return 0;
}

extern "C" int puts (char const* s) { return printf("%s\n", s); }
extern "C" int putchar (int ch) { return printf("%c", ch); }

static void systemReset [[noreturn]] () {
    // ARM Cortex specific
    MMIO32(0xE000ED0C) = (0x5FA<<16) | (1<<2); // SCB AIRCR reset
    while (true) {}
}

extern "C" void abort () {
    printf("abort\n");
    wait_ms(250); // slow down a bit, in case of runaway restarts
    arch::done();
}

extern "C" void __assert_func (char const* f, int l, char const* n, char const* e) {
    printf("\nassert(%s) in %s\n\t%s:%d\n", e, n, f, l);
    abort();
}

extern "C" void __assert (char const* f, int l, char const* e) {
    __assert_func(f, l, "-", e);
}

struct LineSerial : Context {
    Event& incoming;
    char buf [100]; // TODO avoid hard limit for input line length
    uint32_t fill = 0;

    LineSerial () : incoming (*new Event) {
        rxId = incoming.regHandler();
        txId = outQueue.regHandler();
        prevIsr = console.handler();

        console.handler() = []() {
            prevIsr();
            if (console.readable())
                Context::setPending(rxId);
            // TODO this triggers too often, even when there is no TX activity
            if (console.xmit.almostEmpty())
                Context::setPending(txId);
        };

        outFun = writer;
    }

    ~LineSerial () override {
        outFun = outch; // restore non-stacklet-aware version
        console.handler() = prevIsr;
        incoming.deregHandler();
        outQueue.deregHandler();
    }

    auto run () -> bool override {
        incoming.wait();
        incoming.clear();
        while (console.readable()) {
            auto c = console.getc();
            if (c == '\r' || c == '\n') {
                if (fill == 0)
                    continue;
                buf[fill] = 0;
                fill = 0;
                if (!exec(buf))
                    return false;
            } else if (fill < sizeof buf - 1)
                buf[fill++] = c;
        }
        //FIXME broken during switch to fibers: yield();
        return true;
    }

    virtual auto exec (char const*) -> bool = 0;

    static auto writeMax (uint8_t const* ptr, size_t len) -> size_t {
        size_t i = 0;
        while (i < len && console.writable())
            console.putc(ptr[i++]);
        return i;
    }

    static auto writeAll (uint8_t const* ptr, size_t len) -> size_t {
        size_t i = 0;
        while (i < len) {
            i += writeMax(ptr + i, len - i);
            outQueue.wait();
            outQueue.clear();
        }
        return len;
    }

    static void writer (int c) {
        if (current != nullptr)
            writeAll((uint8_t const*) &c, 1);
        else // can only use events when in stacklet context
            outch(c);
    }

    // TODO messy use of static scope
    static void (*prevIsr)();
    static uint8_t rxId, txId;
    static Event outQueue;
};

void (*LineSerial::prevIsr)();
uint8_t LineSerial::rxId;
uint8_t LineSerial::txId;
Event LineSerial::outQueue;

struct HexSerial : LineSerial {
    static constexpr auto PERSIST_SIZE = 2048;

    enum { MagicStart = 987654321, MagicValid = 123456789 };

    auto (*reader)(char const*)->bool;
    IntelHex<32> ihex; // max 32 data bytes per hex input line

    HexSerial (auto (*fun)(char const*)->bool) : reader (fun) {
        auto cmd = bootCmd();
        magic() = 0; // only use saved boot command once
        if (cmd != nullptr)
            exec(persist+4);
    }

    static auto bootCmd () -> char const* {
        if (persist == nullptr)
            persist = (char*) sbrk(4+PERSIST_SIZE); // never freed
        return magic() == MagicValid ? persist+4 : nullptr;
    }

    static auto magic () -> uint32_t& { return *(uint32_t*) persist; }

    auto exec (char const* cmd) -> bool override {
        if (cmd[0] != ':')
            return reader(cmd);
        // Intel hex: store valid data in persist buf, sys reset when done
        ihex.init();
        while (!ihex.parse(*++cmd)) {}
        if (ihex.check == 0 && ihex.type <= 2 &&
                                ihex.addr + ihex.len <= PERSIST_SIZE) {
            switch (ihex.type) {
                case 0: // data bytes
                        if (ihex.addr == 0)
                            magic() = MagicStart;
                        memcpy(persist + 4 + ihex.addr, ihex.data, ihex.len);
                        last = ihex.addr + ihex.len;
                        break;
#if HAS_MRFS
                case 2: // flash offset
                        offset = (uint32_t) mrfsBase +
                                    (ihex.data[0]<<12) + (ihex.data[1]<<4);
                        break;
#endif
                case 1: // end of hex input
                        if (magic() == MagicStart) {
                            if (offset != 0) {
                                // TODO verify proper flashing
                                saveToFlash(offset, persist+4, last);
                                printf("offset %x last %d\n",
                                        (int) offset, (int) last);
                                *persist = 0;
                                offset = 0;
                                break;
                            }
                            magic() = MagicValid;
                        }
                        // fall through
                default: systemReset();
            }
        } else {
            printf("ihex? %d t%d @%04x #%d\n",
                    ihex.check, ihex.type, ihex.addr, ihex.len);
            *persist = 0; // mark persistent buffer as invalid
        }
        return true;
    }

    void saveToFlash (uint32_t off, void const* buf, int len) {
#if STM32F1
        auto pageSz = MMIO16(0x1FFFF7E0) <= 128 ? 1024 : 2048; // use flash sz
        if (off % pageSz == 0)
            Flash::erasePage((void*) off);
        auto words = (uint32_t const*) buf;
        for (int i = 0; i < len; i += 4)
            Flash::write32((void const*) (off+i), words[i/4]);
        Flash::finish();
#elif STM32F4
        (void) off; (void) buf; (void) len; // TODO
#elif STM32L4
        if (off % 2048 == 0) // TODO STM32L4-specific
            Flash::erasePage((void*) off);
        auto words = (uint32_t const*) buf;
        for (int i = 0; i < len; i += 8)
            Flash::write64((void const*) (off+i), words[i/4], words[i/4+1]);
        Flash::finish();
#else
        (void) off; (void) buf; (void) len;
#endif
    }

    uint32_t offset = 0, last = 0;
    static char* persist; // pointer to a buffer which persists across resets
};

char* HexSerial::persist;

struct Command {
    char const* desc;
    union {
        void (*f0)();
        void (*f1)(char const*);
    };
};

static void bc_cmd (char const* cmd) {
    if (strlen(cmd) > 3) {
        HexSerial::magic() = HexSerial::MagicValid;
        strcpy(HexSerial::persist + 4, cmd + 3);
    } else
        HexSerial::magic() = 0;
}

static void bv_cmd () {
    printf("Monty %s (" __DATE__ ", " __TIME__ ")\n", VERSION);
}

static void di_cmd () {
    extern int g_pfnVectors [], _sidata [], _sdata [], _ebss [], _estack [];
    printf("flash 0x%p..0x%p, ram 0x%p..0x%p, stack top 0x%p\n",
            g_pfnVectors, _sidata, _sdata, _ebss, _estack);
    // the following addresses are cpu-family specific
    constexpr uint32_t
#if STM32F1
        cpu = 0xE000ED00, rom = 0x1FFFF7E0, pkg = 0,          uid = 0x1FFFF7E8;
#elif STM32F4
        cpu = 0xE0042000, rom = 0x1FFF7A22, pkg = 0,          uid = 0x1FFF7A10;
#elif STM32F723xx
        cpu = 0xE0042000, rom = 0x1FF07A22, pkg = 0x1FF07BF0, uid = 0x1FF07A10;
#elif STM32F7
        cpu = 0xE000ED00, rom = 0x1FF0F442, pkg = 0,          uid = 0x1FF0F420;
#elif STM32H7
        cpu = 0xE000ED00, rom = 0x1FF1E880, pkg = 0,          uid = 0x1FF1E800;
#elif STM32L0
        cpu = 0xE000ED00, rom = 0x1FF8007C, pkg = 0,          uid = 0x1FF80050;
#elif STM32L4
        cpu = 0xE000ED00, rom = 0x1FFF75E0, pkg = 0x1FFF7500, uid = 0x1FFF7590;
#else
        cpu = 0, rom = 0, pkg = 0, uid = 0;
    return;
#endif
    printf("cpuid 0x%08x, %d kB flash, %d kB ram, package %d\n",
            (int) MMIO32(cpu), MMIO16(rom), (_estack - _sdata) >> 8,
            pkg != 0 ? (int) MMIO32(pkg) & 0x1F : 0); // not always available?
    printf("clock %d kHz, devid %08x-%08x-%08x\n",
            (int) MMIO32(0xE000E014) + 1,
            (int) MMIO32(uid), (int) MMIO32(uid+4), (int) MMIO32(uid+8));
}

static void ec_cmd () {
    Context::current = nullptr;
}

static void ms_cmd () {
    printf("%d\n", (int) ticks);
}

static void pd_cmd () {
    powerDown();
}

static void wd_cmd (char const* cmd) {
    int count = 4095;
    if (strlen(cmd) > 3)
        count = atoi(cmd+3);

    static Iwdg dog;
    dog.reload(count);
    printf("%d ms\n", 8 * count);
}

Command const commands [] = {
    { "bc *  set boot command [cmd ...]"   , (void(*)()) bc_cmd },
    { "bv    show build version"           , bv_cmd },
    { "di    show device info"             , di_cmd },
    { "ec    exit console task"            , ec_cmd },
    { "gc    trigger GC"                   , Context::gcAll },
    { "gr    generate a GC report"         , gcReport },
#if HAS_MRFS
    { "ls    list files in MRFS"           , mrfs::dump },
#endif
    { "ms    current ms tick"              , ms_cmd },
    { "od    object dump"                  , Obj::dumpAll },
    { "pd    power down"                   , pd_cmd },
    { "sr    system reset"                 , systemReset },
    { "vd    vector dump"                  , Vec::dumpAll },
    { "wd N  set watchdog [0..4095] x8 ms" , (void(*)()) wd_cmd },
};

static auto execCmd (char const* buf) -> bool {
    for (auto& cmd : commands)
        if (memcmp(buf, cmd.desc, 2) == 0 && (buf[2] == 0 || buf[2] == ' ')) {
            cmd.f1(buf);
            return Context::current != nullptr;
        }
#if HAS_PYVM
    auto data = vmImport(buf);
    if (data != nullptr) {
        auto p = vmLaunch(data);
        assert(p != nullptr);
        Context::ready.append(p);
        return true;
    }
#endif
    for (auto& cmd : commands)
        printf("  %s\n", cmd.desc);
    return true;
}

auto arch::cliTask() -> Context* {
#if HAS_PYVM
    auto task = vmLaunch((uint8_t const*) HexSerial::bootCmd());
    if (task != nullptr) {
        HexSerial::magic() = 0; // only load saved data once
        return task;
    }
#endif
    return new HexSerial (execCmd);
}

#if HAS_PYVM
auto monty::vmImport (char const* name) -> uint8_t const* {
#if HAS_MRFS
    auto pos = mrfs::find(name);
    return pos >= 0 ? (uint8_t const*) mrfsBase[pos].name : nullptr;
#else
    (void) name;
    return nullptr;
#endif
}
#endif

using namespace altpins;

void arch::init (int size) {
    console.init();
#if STM32F4 || NUCLEO_H743ZI || DEVEBOX_H743
    console.baud(921600, fullSpeedClock()/4);
#elif STM32F723xx || DISCO_F746NG || STM32F469xx || STM32F750xx || STM32F769xx
    console.baud(921600, fullSpeedClock()/2);
#else
    console.baud(921600, fullSpeedClock());
#endif

    setupFaultHandlers();
    printf("\n"); // TODO yuck, the uart TX sends a 0xFF junk char after reset

#if HAS_MRFS
    mrfs::init(mrfsBase, mrfsSize);
    //mrfs::dump();
#endif

    // negative sizes indicate required amount to not allocate
    if (size <= 0) {
        uint8_t top;
        int avail = &top - (uint8_t*) sbrk(0);
        size = avail + (size < -1024 ? size : -1024);
        assert(size > 1024); // there better be at least 1k for the pool
    }

    gcSetup(sbrk(size), size);
    Event::triggers.append(0); // TODO yuck, reserve 1st entry for VM
}

void arch::idle () {
    asm ("wfi");
}

void arch::done () {
    //monty::Context::gcAll();
    HexSerial::magic() = 0; // clear boot command buffer
    wait_ms(10);
    systemReset(); // will resume the cli task with a clean slate
}

#if STM32L0

extern "C" unsigned int __atomic_fetch_or_4 (void volatile* p, unsigned int v, int o) {
    // see https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html
    // FIXME this version is not atomic!
    (void) o;
    auto q = (uint32_t volatile*) p;
    // atomic start
    auto t = *q;
    *q |= v;
    // atomic end
    return t;
}

extern "C" unsigned int __atomic_fetch_and_4 (void volatile* p, unsigned int v, int o) {
    // see https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html
    // FIXME this version is not atomic!
    (void) o;
    auto q = (uint32_t volatile*) p;
    // atomic start
    auto t = *q;
    *q &= v;
    // atomic end
    return t;
}

#endif

#ifdef UNIT_TEST
extern "C" {
    void unittest_uart_begin () { arch::init(1024); wait_ms(100); }
    void unittest_uart_putchar (char c) { console.putc(c); }
    void unittest_uart_flush () { while (!console.xmit.empty()) {} }
    void unittest_uart_end () { while (true) {} }
}
#endif
