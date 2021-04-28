#include "hall.h"
#include <cstring>

extern uint32_t SystemCoreClock; // CMSIS

namespace hall {
    uint8_t irqMap [100]; // TODO size according to real vector table
    Device* devMap [20];  // must be large enough to hold all device objects
    uint8_t devNext;

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

    namespace watchdog {  // [1] pp.495
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

    namespace rtc {
        constexpr auto RTC = io32<0x4000'2800>;
        constexpr auto PWR = io32<0x4000'7000>;
        enum { TR=0x00,DR=0x04,CR=0x08,ISR=0x0C,WPR=0x24,BKPR=0x50 };
        enum { BDCR=0x70 };

        void init () {
            RCC(0x40)[28] = 1; // enable PWREN
            PWR(0x00)[8] = 1;  // set DBP [1] p.481

            RCC(BDCR)[0] = 1;             // LSEON backup domain
            while (RCC(BDCR)[1] == 0) {}  // wait for LSERDY
            RCC(BDCR)[8] = 1;             // RTSEL = LSE
            RCC(BDCR)[15] = 1;            // RTCEN
        }

        auto get () -> DateTime {
            RTC(WPR) = 0xCA;  // disable write protection, [1] p.803
            RTC(WPR) = 0x53;

            RTC(ISR)[5] = 0;              // clear RSF
            while (RTC(ISR)[5] == 0) {}   // wait for RSF

            RTC(WPR) = 0xFF;  // re-enable write protection

            // shadow registers are now valid and won't change while being read
            uint32_t tod = RTC(TR);
            uint32_t doy = RTC(DR);

            DateTime dt;
            dt.ss = (tod & 0xF) + 10 * ((tod>>4) & 0x7);
            dt.mm = ((tod>>8) & 0xF) + 10 * ((tod>>12) & 0x7);
            dt.hh = ((tod>>16) & 0xF) + 10 * ((tod>>20) & 0x3);
            dt.dy = (doy & 0xF) + 10 * ((doy>>4) & 0x3);
            dt.mo = ((doy>>8) & 0xF) + 10 * ((doy>>12) & 0x1);
            // works until end 2063, will fail (i.e. roll over) in 2064 !
            dt.yr = ((doy>>16) & 0xF) + 10 * ((doy>>20) & 0x7);
            return dt;
        }

        void set (DateTime dt) {
            RTC(WPR) = 0xCA;  // disable write protection, [1] p.803
            RTC(WPR) = 0x53;

            RTC(ISR)[7] = 1;             // set INIT
            while (RTC(ISR)[6] == 0) {}  // wait for INITF
            RTC(TR) = (dt.ss + 6 * (dt.ss/10)) |
                ((dt.mm + 6 * (dt.mm/10)) << 8) |
                ((dt.hh + 6 * (dt.hh/10)) << 16);
            RTC(DR) = (dt.dy + 6 * (dt.dy/10)) |
                ((dt.mo + 6 * (dt.mo/10)) << 8) |
                ((dt.yr + 6 * (dt.yr/10)) << 16);
            RTC(ISR)[7] = 0;             // clear INIT

            RTC(WPR) = 0xFF;  // re-enable write protection
        }

        // access to the backup registers

        auto getData (int reg) -> uint32_t {
            return RTC(BKPR + 4*reg);  // regs 0..31
        }

        void setData (int reg, uint32_t val) {
            RTC(BKPR + 4*reg) = val;  // regs 0..31
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
