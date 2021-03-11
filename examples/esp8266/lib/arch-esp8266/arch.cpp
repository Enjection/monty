#include <monty.h>
#include "arch.h"

#include <cassert>
#include <cstdio>
#include <unistd.h>

#include "esp_common.h"
#include "freertos/task.h"
#include "gpio.h"

using namespace monty;

void wait_ms (uint32_t ms) {
    vTaskDelay(ms / portTICK_RATE_MS);
}

#if 0
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
#endif

struct LineSerial : Stacklet {
    Event& incoming;
    char buf [100]; // TODO avoid hard limit for input line length
    uint32_t fill = 0;

    LineSerial () : incoming (*new Event) {
        rxId = incoming.regHandler();
        txId = outQueue.regHandler();
    }

    ~LineSerial () override {
        incoming.deregHandler();
        outQueue.deregHandler();
    }

    auto run () -> bool override {
        incoming.wait();
        incoming.clear();
        while (true) {
            auto c = getchar();
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
        yield();
        return true;
    }

    virtual auto exec (char const*) -> bool = 0;

    static auto writeMax (uint8_t const* ptr, size_t len) -> size_t {
        size_t i = 0;
        while (i < len)
            putchar(ptr[i++]);
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

    // TODO messy use of static scope
    static uint8_t rxId, txId;
    static Event outQueue;
};

uint8_t LineSerial::rxId;
uint8_t LineSerial::txId;
Event LineSerial::outQueue;

#if 1
struct HexSerial : LineSerial {
    auto (*reader)(char const*)->bool;

    HexSerial (auto (*fun)(char const*)->bool) : reader (fun) {}

    auto exec (char const* cmd) -> bool override {
        return reader(cmd);
    }
};
#else
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
                                printf("offset %x last %d\n", offset, last);
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
#if STM32L4
        if (off % 2048 == 0) // TODO STM32L4-specific
            Flash::erasePage((void*) off);
        auto words = (uint32_t const*) buf;
        for (int i = 0; i < len; i += 8)
            Flash::write64((void const*) (off+i), words[i/4], words[i/4+1]);
        Flash::finish();
#endif
    }

    uint32_t offset = 0, last = 0;
    static char* persist; // pointer to a buffer which persists across resets
};

char* HexSerial::persist;
#endif

struct Command {
    char const* desc;
    union {
        void (*f0)();
        void (*f1)(char const*);
    };
};

static void bv_cmd () {
    printf("Monty %s (" __DATE__ ", " __TIME__ ")\n", VERSION);
}

static void di_cmd () {
    system_print_meminfo();
    printf(" sdk %s\n", system_get_sdk_version());
    printf("  id %u\n", system_get_chip_id());
    printf("time %u\n", system_get_time());
    printf(" rtc %u\n", system_get_rtc_time());
    printf("freq %u\n", system_get_cpu_freq());
    printf(" vdd %u\n", system_get_vdd33());
    printf("size %u\n", system_get_flash_size_map());
    printf("free %u\n", system_get_free_heap_size());
}

Command const commands [] = {
    { "bv    show build version"           , bv_cmd },
    { "di    show device info"             , di_cmd },
    { "gc    trigger garbage collection"   , Stacklet::gcAll },
    { "gr    generate a GC report"         , gcReport },
    { "od    object dump"                  , Obj::dumpAll },
    { "vd    vector dump"                  , Vec::dumpAll },
};

static auto execCmd (char const* buf) -> bool {
    for (auto& cmd : commands)
        if (memcmp(buf, cmd.desc, 2) == 0 && (buf[2] == 0 || buf[2] == ' ')) {
            cmd.f1(buf);
            return true;
        }
#if HAS_PYVM
    auto data = vmImport(buf);
    if (data != nullptr) {
        auto p = vmLaunch(data);
        assert(p != nullptr);
        Stacklet::ready.append(p);
        return true;
    }
#endif
    for (auto& cmd : commands)
        printf("  %s\n", cmd.desc);
    return true;
}

auto arch::cliTask() -> Stacklet* {
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

void task_blink (void*) {
    printf("Hello world!\n");
    di_cmd();
    wait_ms(1000);
    vTaskDelete(nullptr);
}

void arch::init (int size) {
#if 0
    // negative sizes indicate required amount to not allocate
    if (size <= 0) {
        uint8_t top;
        int avail = &top - (uint8_t*) sbrk(0);
        size = avail + (size < -1024 ? size : -1024);
        assert(size > 1024); // there better be at least 1k for the pool
    }

    gcSetup(malloc(size), size);
    Event::triggers.append(0); // TODO yuck, reserve 1st entry for VM
#endif
xTaskCreate(&task_blink, (signed char const*) "startup", 2048, nullptr, 1, nullptr);
}

void arch::idle () {
    taskYIELD();
}

void arch::done () {
    //monty::Stacklet::gcAll();
    wait_ms(10);
    system_deep_sleep(0);
}

/* FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABCCC
 *                A : rf cal
 *                B : rf init data
 *                C : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
 */
extern "C" uint32 user_rf_cal_sector_set () {
    flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;
    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;

        default:
            rf_cal_sec = 0;
            break;
    }

    return rf_cal_sec;
}

extern "C" uint32_t __atomic_fetch_or_4 (void volatile* p, uint32_t v, int o) {
    // see https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html
    // FIXME this version is not atomic!
    auto q = (uint32_t volatile*) p;
    // atomic start
    auto t = *q;
    *q |= v;
    // atomic end
    return t;
}

extern "C" uint32_t __atomic_fetch_and_4 (void volatile* p, uint32_t v, int o) {
    // see https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html
    // FIXME this version is not atomic!
    auto q = (uint32_t volatile*) p;
    // atomic start
    auto t = *q;
    *q &= v;
    // atomic end
    return t;
}

#ifdef UNIT_TEST
extern "C" {
    void unittest_uart_begin () { arch::init(1024); }
    void unittest_uart_putchar (char c) { putchar(c); }
    void unittest_uart_flush () {}
    void unittest_uart_end () { while (true) {} }
}
#endif
