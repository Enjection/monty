#include "hall.h"
#include <cstring>

extern uint32_t SystemCoreClock; // CMSIS

namespace hall {
    uint8_t irqMap [100]; // TODO size according to real vector table
    Device* devMap [20];  // must be large enough to hold all device objects
    uint8_t devNext;
    volatile uint32_t Device::pending;

    constexpr auto SCB  = io32<0xE000'E000>;

    void idle () {
        asm ("wfi");
    }

    auto Pin::mode (int m) const -> int {
#if STM32F1
        RCC(0x18) |= (1<<_port) | (1<<0); // enable GPIOx and AFIO clocks
        x // FIXME wrong, mode bits have changed, and it changes return value
        if (m == 0b1000 || m == 0b1100) {
            gpio32(ODR)[_pin] = m & 0b0100;
            m = 0b1000;
        }
        gpio32(0x00).mask(4*_pin, 4) = m; // CRL/CRH
#else
        // enable GPIOx clock
#if STM32F3
        RCC(0x14)[_port] = 1;
#elif STM32F4 | STM32F7
        RCC(0x30)[_port] = 1;
        asm ("dsb");
#elif STM32G0
        RCC(0x34)[_port] = 1;
#elif STM32H7
        RCC(0xE0)[_port] = 1;
        asm ("dsb");
#elif STM32L0
        RCC(0x2C)[_port] = 1;
#elif STM32L4
        RCC(0x4C)[_port] = 1;
#endif

        gpio32(0x00).mask(2*_pin, 2) = m;      // MODER
        gpio32(0x04).mask(  _pin, 1) = m >> 2; // TYPER
        gpio32(0x08).mask(2*_pin, 2) = m >> 3; // OSPEEDR
        gpio32(0x0C).mask(2*_pin, 2) = m >> 5; // PUPDR
        gpio32(0x20).mask(4*_pin, 4) = m >> 8; // AFRL/AFRH
#endif // STM32F1
        return m;
    }

    auto Pin::mode (char const* desc) const -> int {
        int m = 0, a = 0;
        for (auto s = desc; *s != ',' && *s != 0; ++s)
            switch (*s) {     // 1 pp ss t mm
                case 'A': m  = 0b1'00'00'0'11; break; // m=11 analog
                case 'F': m  = 0b1'00'00'0'00; break; // m=00 float
                case 'D': m |= 0b1'10'00'0'00; break; // m=00 pull-down
                case 'U': m |= 0b1'01'00'0'00; break; // m=00 pull-up

                case 'P': m  = 0b1'00'01'0'01; break; // m=01 push-pull
                case 'O': m  = 0b1'00'01'1'01; break; // m=01 open drain

                case 'L': m &= 0b1'11'00'1'11; break; // s=00 low speed
                case 'N':                      break; // s=01 normal speed
                case 'H': m ^= 0b0'00'11'0'00; break; // s=10 high speed
                case 'V': m |= 0b0'00'10'0'00; break; // s=11 very high speed

                default:  if (*s < '0' || *s > '9' || a > 1) return -1;
                          m = (m & ~0b11) | 0b10;   // m=10 alt mode
                          a = 10 * a + *s - '0';
                case ',': break; // valid as terminator
            }
        return mode(m + (a<<8));
    }

    auto Pin::config (char const* desc) -> int {
        if ('A' <= *desc && *desc <= 'O') {
            _port = *desc++ - 'A';
            _pin = 0;
            while ('0' <= *desc && *desc <= '9')
                _pin = 10 * _pin + *desc++ - '0';
        }
        return _port == 15 ? -1 : *desc++ != ':' ? 0 : mode(desc);
    }

    auto Pin::define (char const* d, Pin* v, int n) -> char const* {
        Pin dummy;
        int lastMode = 0;
        while (*d != 0) {
            if (--n < 0)
                v = &dummy;
            auto m = v->config(d);
            if (m < 0)
                break;
            if (m != 0)
                lastMode = m;
            else if (lastMode != 0)
                v->mode(lastMode);
            auto p = strchr(d, ',');
            if (p == nullptr)
                return n > 0 ? d : nullptr;
            d = p+1;
            ++v;
        }
        return d;
    }

    Device::Device () {
        //TODO ensure(devNext < sizeof devMap / sizeof *devMap);
        _id = devNext;
        devMap[devNext++] = this;
    }

    void Device::irqInstall (uint32_t irq) const {
        //TODO ensure(irq < sizeof irqMap);
        irqMap[irq] = _id;
        nvicEnable(irq);
    }

    void Device::processAllPending () {
        uint32_t pend;
        {
            BlockIRQ crit;
            pend = pending;
            pending = 0;
        }
        for (int i = 0; i < devNext; ++i)
            if (pend & (1<<i))
                devMap[i]->process();
    }

    auto systemHz () -> uint32_t {
        return SystemCoreClock;
    }

    void systemReset () {
        SCB(0xD0C) = (0x5FA<<16) | (1<<2); // SCB AIRCR reset
        while (true) {}
    }

    namespace systick {
        volatile uint32_t ticks;
        uint8_t rate;
        void (*handler) () = [] {};

        void init (uint8_t ms) {
            if (rate > 0)
                ticks = millis();
            rate = ms;
            SCB(0x14) = (ms*(systemHz()/1000))/8-1; // reload value
            SCB(0x18) = 0;                          // current
            SCB(0x10) = 0b011;                      // control, ÷8 mode
        }

        void deinit () {
            ticks = millis();
            SCB(0x10) = 0;
            rate = 0;
        }

        auto millis () -> uint32_t {
            while (true) {
                uint32_t t = ticks, n = SCB(0x18);
                if (t == ticks)
                    return t + rate - (n*8)/(systemHz()/1000);
            } // ticked just now, spin one more time
        }

        auto micros () -> uint32_t {
            // scaled to work with any clock rate multiple of 100 kHz
            while (true) {
                uint32_t t = ticks, n = SCB(0x18);
                if (t == ticks)
                    return (t%1000 + rate)*1000 - (n*80)/(systemHz()/100'000);
            } // ticked just now, spin one more time
        }

        extern "C" void SysTick_Handler () {
            ticks += rate;
            handler();
        }
    }

    namespace cycles {
        enum { CTRL=0x000,CYCCNT=0x004,LAR=0xFB0 };
        enum { DEMCR=0xDFC };

        void init () {
            DWT(LAR) = 0xC5ACCE55;
            SCB(DEMCR) = SCB(DEMCR) | (1<<24); // set TRCENA in DEMCR
            clear();
            DWT(CTRL) = DWT(CTRL) | 1;
        }

        void deinit () {
            DWT(CTRL) = DWT(CTRL) & ~1;
        }
    }

    namespace watchdog {
        constexpr auto IWDG = io32<0x4000'3000>;
        enum { KR=0x00,PR=0x04,RLR=0x08,SR=0x0C };

        uint8_t cause; // "semi public"

        auto resetCause () -> int {
#if STM32F4 || STM32F7
            enum { CSR=0x74, RMVF=24 };
#elif STM32L4
            enum { CSR=0x94, RMVF=23 };
#endif
            if (cause == 0) {
                cause = RCC(CSR) >> 24;
                RCC(CSR)[RMVF] = 1; // clears all reset-cause flags
            }
            return cause & (1<<5) ? -1 :     // iwdg
                   cause & (1<<3) ? 2 :      // por/bor
                   cause & (1<<2) ? 1 : 0;   // nrst, or other
        }

        void init (int rate) {
            while (IWDG(SR)[0]) {}  // wait until !PVU
            IWDG(KR) = 0x5555;      // unlock PR
            IWDG(PR) = rate;        // max timeout, 0 = 400ms, 7 = 26s
            IWDG(KR) = 0xCCCC;      // start watchdog
        }
        void reload (int n) {
            while (IWDG(SR)[1]) {}  // wait until !RVU
            IWDG(KR) = 0x5555;      // unlock PR
            IWDG(RLR) = n;
            kick();
        }
        void kick () {
            IWDG(KR) = 0xAAAA;      // reset the watchdog timout
        }
    }

    void debugPutc (void*, int c) {
        constexpr auto ITM8 = io8<0xE000'0000>;
        constexpr auto ITM = io32<0xE000'0000>;
        enum { TER=0xE00, TCR=0xE80, LAR=0xFB0 };

        if (ITM(TCR)[0] && ITM(TER)[0]) {
            while (ITM(0)[0] == 0) {}
            ITM8(0) = c;
        }
    }

    extern "C" void irqDispatch () {
        uint8_t irq = SCB(0xD04); // ICSR
        auto idx = irqMap[irq-16];
        devMap[idx]->interrupt();
    }
}

// to re-generate "irqs.h", see the "all-irqs.sh" script

#define IRQ(f) void f () __attribute__ ((alias ("irqDispatch")));
extern "C" {
#include "irqs.h"
}
