#include <cstdint>
#include <cstring>

namespace device {
#include "prelude.h"
}

namespace altpins {
#include "altpins.h"
}

// see https://interrupt.memfault.com/blog/asserts-in-embedded-systems
#ifdef NDEBUG
#define ensure(exp) ((void)0)
#else
#define ensure(exp)                                 \
  do                                                \
    if (!(exp)) {                                   \
      void *pc;                                     \
      asm volatile ("mov %0, pc" : "=r" (pc));      \
      void const* lr = __builtin_return_address(0); \
      mcu::failAt(pc, lr);                          \
    }                                               \
  while (false)
#endif

extern struct Printer* stdOut;
extern "C" int printf (char const*, ...);

namespace mcu {
    enum ARM_Family { STM_F4, STM_F7, STM_L0, STM_L4 };
#if STM32F4
    constexpr auto FAMILY = STM_F4;
#elif STM32F7
    constexpr auto FAMILY = STM_F7;
#elif STM32L0
    constexpr auto FAMILY = STM_L0;
#elif STM32L4
    constexpr auto FAMILY = STM_L4;
#endif

    void debugf (const char* fmt, ...); // hard-wired to swoOut
    auto snprintf (char*, uint32_t, const char*, ...) -> int;

    auto micros () -> uint32_t;
    auto millis () -> uint32_t;
    void msWait (uint16_t ms);
    auto systemClock () -> uint32_t;
    auto fastClock (bool pll =true) -> uint32_t;
    auto slowClock (bool low =true) -> uint32_t;

    void powerDown (bool standby =true);
    [[noreturn]] void systemReset ();
    [[noreturn]] void failAt (void const*, void const*) __attribute__ ((weak));

    struct IOWord {
        uint32_t volatile& addr;

        operator uint32_t () const { return addr; }
        void operator= (uint32_t v) const { addr = v; }

#if STM32L0 || STM32F7
        // simulated bit-banding, works with any address, but not atomic
        struct IOBit {
            uint32_t volatile& addr;
            uint8_t bit;

            operator uint32_t () const { return (addr >> bit) & 1; }
            void operator= (uint32_t v) const {
                if (v) addr |= (1<<bit); else addr &= ~(1<<bit);
            }
        };

        auto operator[] (int b) {
            return IOBit {(&addr)[b>>5], (uint8_t) (b & 0x1F)};
        }
#else
        // use bit-banding, only works for specific RAM and periperhal areas
        auto& operator[] (int b) {
            auto a = (uint32_t) &addr;
            return *(uint32_t volatile*)
                ((a & 0xF000'0000) + 0x0200'0000 + (a << 5) + (b << 2));
        }
#endif
        
        struct IOMask {
            uint32_t volatile& addr;
            uint8_t bit, width;

            operator uint32_t () const {
                auto mask = (1<<width)-1;
                return (addr >> bit) & mask;
            }

            void operator= (uint32_t v) const {
                auto mask = (1<<width)-1;
                addr = (addr & ~(mask<<bit)) | ((v & mask)<<bit);
            }
        };

        auto mask (int b, uint8_t w) {
            return IOMask {(&addr)[b>>5], (uint8_t) (b & 0x1F), w};
        }
    };

    template <uint32_t A>
    auto io32 (int off =0) {
        return IOWord {*(uint32_t volatile*) (A+off)};
    }
    template <uint32_t A>
    auto& io16 (int off =0) {
        return *(uint16_t volatile*) (A+off);
    }
    template <uint32_t A>
    auto& io8 (int off =0) {
        return *(uint8_t volatile*) (A+off);
    }

    #include "hw-map.h"

    struct Pin {
        uint8_t _port :4, _pin :4;

        constexpr Pin () : _port (15), _pin (0) {}

#if STM32F1
        enum { CRL=0x00, CRH=0x04, IDR=0x08, ODR=0x0C, BSRR=0x10, BRR=0x14 };
#else
        enum { MODER=0x00, TYPER=0x04, OSPEEDR=0x08, PUPDR=0x0C, IDR=0x10,
                ODR=0x14, BSRR=0x18, AFRL=0x20, AFRH=0x24, BRR=0x28 };
#endif

        auto read () const { return (gpio32(IDR)>>_pin) & 1; }
        void write (int v) const { gpio32(BSRR) = (v ? 1 : 1<<16)<<_pin; }

        // shorthand
        void toggle () const { write(read() ^ 1); }
        operator int () const { return read(); }
        void operator= (int v) const { write(v); }

        // pin definition string: [A-O][<pin#>]:[AFPO][DU][LNHV][<alt#>][,]
        // return -1 on error, 0 if no mode set, or the mode (always > 0)
        auto define (char const* desc) -> int;

        // define multiple pins, returns nullptr if ok, else ptr to error
        static auto define (char const* d, Pin* v, int n) -> char const*;
    private:
        auto gpio32 (int off) const -> IOWord { return GPIO(0x400*_port+off); }
        auto isValid () const { return _port < 15; }
        auto mode (int) const -> int;
        auto mode (char const* desc) const -> int;
    };

    struct SpiGpio {
        Pin _mosi, _miso, _sclk, _nsel;
        uint8_t _cpol =0;

        void init (char const* defs) {
            Pin pins [4];
            Pin::define(defs, pins, 4);
            Pin::define(":PV,:F,:PV,:PV", pins, 4);
            _mosi = pins[0]; _miso = pins[1]; _sclk = pins[2]; _nsel = pins[3];

            disable();
            _sclk = _cpol;
        }

        void enable () const { _nsel = 0; }
        void disable () const { _nsel = 1; }

        auto xfer (uint8_t v) const -> uint8_t {
            for (int i = 0; i < 8; ++i) {
                _mosi = v & 0x80;
                v <<= 1;
                _sclk = !_cpol;
                v |= _miso;
                _sclk = _cpol;
            }
            return v;
        }

        void read (uint8_t* buf, int len) const {
            enable();
            for (int i = 0; i < len; ++i)
                buf[i] = xfer(0);
            disable();
        }

        void write (uint8_t const* buf, int len) const {
            enable();
            for (int i = 0; i < len; ++i)
                xfer(buf[i]);
            disable();
        }
    };

    struct SpiFlash : private SpiGpio {
        using SpiGpio::init;

        void reset () {
            cmd(0x66); disable(); cmd(0x99); disable();
        }

        auto devId () {
            cmd(0x9F);
            int r = xfer(0) << 16; r |= xfer(0) << 8; r |= xfer(0);
            disable();
            return r;
        }

        auto size () { // e.g. W25Q64: 0xC84017 => 8192 KB
            return 1 << ((devId()&0x1F) - 10);
        }

        void wipe () { // 0x60 doesn't work on Micron Tech (N25Q)
            wcmd(0xC7); wait();
        }

        void erase (int offset) {
            wcmd(0x20); w24b(offset); wait();
        }

        void read (int offset, uint8_t* buf, int len) {
            cmd(0x0B); w24b(offset); xfer(0); SpiGpio::read(buf, len);
        }

        void write (int offset, uint8_t const* buf, int len) {
            wcmd(0x02); w24b(offset); SpiGpio::write(buf, len); wait();
        }

    private:
        void cmd (int arg) {
            enable(); xfer(arg);
        }
        void wait () {
            disable(); cmd(0x05); while (xfer(0) & 1) {} disable();
        }
        void wcmd (int arg) {
            wait(); cmd(0x06); disable(); cmd(arg);
        }
        void w24b (int offset) {
            xfer(offset >> 16); xfer(offset >> 8); xfer(offset);
        }
    };

    struct BlockIRQ {
        BlockIRQ () { asm ("cpsid i"); }
        ~BlockIRQ () { asm ("cpsie i"); }
    };

    void idle () __attribute__ ((weak)); // called with interrupts disabled

    struct Device {
        uint8_t _id;

        Device ();
        ~Device ();

        template <typename F>
        void waitWhile (F fun) {
            while (true) {
                BlockIRQ crit;
                if (!fun())
                    return;
                if (pending & (1<<_id)) {
                    pending ^= 1<<_id;
                    idle();
                }
            }
        }

        // install a IRQ dispatch handler in the hardware IRQ vector
        void irqInstall (uint32_t irq);

        virtual void irqHandler () =0;         // called at interrupt time
        void trigger () { pending |= 1<<_id; } // called at interrupt time

        static void nvicEnable (uint8_t irq) {
            NVIC(4*(irq>>5)) = 1 << (irq & 0x1F);
        }

        static void nvicDisable (uint8_t irq) {
            NVIC(0x80+4*(irq>>5)) = 1 << (irq & 0x1F);
        }

        static auto clearPending () {
            BlockIRQ crit;
            auto r = pending;
            pending = 0;
            return r;
        }

        static uint32_t pending;
        static uint8_t irqMap [];
        static Device* devMap [];
    };

    using namespace device;
    using namespace altpins;
#if STM32F4 || STM32F7
    #include "uart-stm32f4f7.h"
#elif STM32L4
    #include "uart-stm32l4.h"
#endif

    struct Serial : Uart {
        Serial (int num, char const* pins =nullptr) : Uart (num) {
            if (pins != nullptr) {
                mcu::Pin txrx [2];
                Pin::define(pins, txrx, 2);
                init();
            }
        }

        struct Chunk { uint8_t* buf; uint16_t len; };

        auto recv () -> Chunk {
            uint16_t end;
            waitWhile([&]() {
                end = rxNext();
                return rxPull == end; // true if there's no data
            });
            if (end < rxPull)
                end = sizeof rxBuf;
            return {rxBuf+rxPull, (uint16_t) (end-rxPull)};
        }

        void didRecv (uint32_t len) {
            rxPull = (rxPull + len) % sizeof rxBuf;
        }

        // there are 3 cases (note: #=sending, +=pending, .=free)
        //
        // txBuf: [   <-txLeft   txLast   txNext   ]
        //        [...#################+++++++++...]
        //
        // txBuf: [   txLast   txNext   <-txLeft   ]
        //        [#########+++++++++...###########]
        //
        // txBuf: [   txNext   <-txLeft   txLast   ]
        //        [+++++++++...#################+++]
        //
        // txLeft() reads the "CNDTR" DMA register, which counts down to zero
        // it's relative to txLast, which marks the current transfer limit
        //
        // when the DMA is done and txNext != txLast, a new xfer is started
        // this happens at interrupt time and also adjusts txLast accordingly
        // if txLast > txNext, two separate transfers need to be started

        auto canSend () -> Chunk {
            uint16_t avail;
            waitWhile([&]() {
                auto left = txLeft();
                ensure(left < sizeof txBuf);
                auto take = txWrap(txNext + sizeof txBuf - left);
                avail = take > txNext ? take - txNext : sizeof txBuf - txNext;
                return left != 0 || avail == 0;
            });
            ensure(avail > 0);
            return {txBuf+txNext, avail};
        }

        void send (uint8_t const* p, uint32_t n) {
            while (n > 0) {
                auto [ptr, len] = canSend();
                if (len > n)
                    len = n;
                memcpy(ptr, p, len);
                txNext = txWrap(txNext + len);
                if (txLeft() == 0)
                    txStart();
                p += len;
                n -= len;
            }
        }

    private:
        uint16_t rxPull =0;
    };
}
