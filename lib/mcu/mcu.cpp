#include "mcu.h"
#include <cstring>

#include <cstdarg>
#include "printer.h"

// from CMSIS
extern uint32_t SystemCoreClock;
extern "C" void SystemCoreClockUpdate ();

extern "C" void abort () { ensure(0); }

// this appears to be used as placeholder in abstract class destructors
extern "C" void __cxa_pure_virtual () __attribute__ ((alias ("abort")));
// also bypass the std::{get,set}_terminate logic, and just abort
namespace std { void terminate () __attribute__ ((alias ("abort"))); }

Printer swoOut (nullptr, [](void*, uint8_t const* ptr, int len) {
    using namespace mcu;
    constexpr auto ITM8 =  io8<0xE000'0000>;
    constexpr auto ITM  = io32<0xE000'0000>;
    enum { TER=0xE00, TCR=0xE80, LAR=0xFB0 };

    if (ITM(TCR)[0] && ITM(TER)[0])
        while (--len >= 0) {
            while (ITM(0)[0] == 0) {}
            ITM8(0) = *ptr++;
        }
});

Printer* stdOut = &swoOut;

int printf (const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int result = stdOut->vprintf(fmt, ap);
    va_end(ap);
    return result;
}

extern "C" int puts (char const* s) { return printf("%s\n", s); }
extern "C" int putchar (int ch) { return printf("%c", ch); }

namespace mcu {
    SmallBuf smallBuf;
    uint8_t Stream::eof;
    uint32_t Device::pending;
    uint8_t Device::irqMap [(int) device::IrqVec::limit];
    Device* Device::devMap [20]; // large enough to handle all device objects
    uint32_t volatile ticks;

    void systemReset () {
        mcu::SCB(0xD0C) = (0x5FA<<16) | (1<<2); // SCB AIRCR reset
        while (true) {}
    }

    void failAt (void const* pc, void const* lr) { // weak, can be redefined
        printf("failAt %p %p\n", pc, lr);
        for (uint32_t i = 0; i < systemClock() >> 15; ++i) {}
        systemReset();
    }

    void idle () { asm ("wfi"); } // weak idle handler, can be redefined

    void debugf (const char* fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        swoOut.vprintf(fmt, ap);
        va_end(ap);
    }

    auto snprintf (char* buf, uint32_t len, const char* fmt, ...) -> int {
        struct Info { char* p; int n; } info {buf, (int) len};

        Printer sprinter (&info, [](void* arg, uint8_t const* ptr, int len) {
            auto& info = *(struct Info*) arg;
            while (--len >= 0 && --info.n > 0)
                *info.p++ = *ptr++;
        });

        va_list ap;
        va_start(ap, fmt);
        int result = sprinter.vprintf(fmt, ap);
        va_end(ap);

        if (len > 0)
            *info.p = 0;
        return result;
    }

    uint8_t *reserveNext, *reserveLimit;

    auto reserveNonCached (int bits) -> uint32_t {
        constexpr auto slack = 16;
        const int size = 1<<bits;
        const int mask = size - 1;

        reserveLimit = (uint8_t*) &mask + slack;
        reserveNext = (uint8_t*) ((uint32_t) reserveLimit & ~mask);

        constexpr auto MPU = io32<0xE000'ED90>;
        enum { TYPE=0x00,CTRL=0x04,RNR=0x08,RBAR=0x0C,RASR=0x10 };

        MPU(CTRL) = (1<<2) | (1<<0); // PRIVDEFENA ENABLE

        {
            asm volatile ("dsb \n isb");
            //BlockIRQ crit;

            MPU(RBAR) = (uint32_t) reserveNext | (1<<4); // ADDR VALID REGION:0
            MPU(RASR) = // XN AP:FULL TEX S SIZE ENABLE
                (1<<28)|(3<<24)|(1<<19)|(1<<18)|((bits-1)<<1)|(1<<0);

            asm volatile ("isb \n dsb \n dmb");
        }

        debugf("uncache %08x..%08x, %d b\n", reserveNext, reserveNext + size, size);
        return ((uint32_t) &mask & mask) + slack;
    }

    auto allocateNonCached (uint32_t sz) -> void* {
        ensure(reserveNext != nullptr);
        auto p = reserveNext;
        sz += 3 & -sz; // round up to multiple of 4
        ensure(p + sz <= reserveLimit);
        reserveNext = p + sz;
        return p;
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

    auto Pin::define (char const* desc) -> int {
        if ('A' <= *desc && *desc <= 'O') {
            _port = *desc++ - 'A';
            _pin = 0;
            while ('0' <= *desc && *desc <= '9')
                _pin = 10 * _pin + *desc++ - '0';
        }
        return !isValid() ? -1 : *desc++ != ':' ? 0 : mode(desc);
    }

    auto Pin::define (char const* d, Pin* v, int n) -> char const* {
        int lastMode = 0;
        while (--n >= 0) {
            auto m = v->define(d);
            if (m < 0)
                break;
            if (m != 0)
                lastMode = m;
            else if (lastMode != 0)
                v->mode(lastMode);
            ++v;
            auto p = strchr(d, ',');
            if (n == 0)
                return *d != 0 ? p : nullptr; // point to comma if more
            if (p != nullptr)
                d = p+1;
            else if (lastMode == 0)
                break;
        }
        return d;
    }

    Device::Device () {
        for (auto& e : devMap)
            if (e == nullptr) {
                e = this;
                _id = &e - devMap;
                return;
            }
        ensure(0); // ran out of unused device slots
    }

    Device::~Device () {
        devMap[_id] = nullptr;
        // TODO clear irqMap entries and NVIC enables
    }

    void Device::irqInstall (uint32_t irq) {
        ensure(irq < sizeof irqMap);
        irqMap[irq] = _id;
        nvicEnable(irq);
    }

    auto systemClock () -> uint32_t {
        return SystemCoreClock;
    }

    extern "C" void SysTick_Handler () {
        ++ticks;
    }

    void msWait (uint16_t ms) {
        while (ms > 10) {
            msWait(10); // TODO crude way to avoid 24-bit overflow in systick
            ms -= 10;
        }

        auto hz = systemClock();
        SCB(0x14) = ms*(hz/1000)-1; // reload
        SCB(0x18) = 0;              // current
        SCB(0x10) = 0b111;          // control & status

        auto t = ticks;
        while (true) {
            BlockIRQ crit;
            if (t != ticks)
                break;
            idle();
        }
        ticks = t + ms;
    }

    auto millis () -> uint32_t {
        return ticks;
    }

#if STM32F7
    void enableClkWithPLL () { // using internal 16 MHz HSI
        constexpr auto MHZ = F_CPU/1000000;
        FLASH(0x00) = 0x300 + MHZ/30;           // flash ACR, 0..7 wait states
        RCC(0x00)[16] = 1;                      // HSEON
        while (RCC(0x00)[17] == 0) {}           // wait for HSERDY
        RCC(0x08) = (4<<13) | (5<<10) | (1<<0); // prescaler w/ HSE
        RCC(0x04) = (8<<24) | (1<<22) | (0<<16) | (2*MHZ<<6) | (XTAL<<0);
        RCC(0x00)[24] = 1;                      // PLLON
        while (RCC(0x00)[25] == 0) {}           // wait for PLLRDY
        RCC(0x08) = (0b100<<13) | (0b101<<10) | (0b10<<0);
    }

    auto fastClock (bool pll) -> uint32_t {
        (void) pll; // TODO ignored
        // TODO set LDO voltage range, and over-drive if needed
        enableClkWithPLL();
        return SystemCoreClock = F_CPU;
    }
#elif STM32L4
    void enableClkWithPLL () { // using internal 16 MHz HSI
        FLASH(0x00) = 0x704;                    // flash ACR, 4 wait states
        RCC(0x00)[8] = 1;                       // HSION
        while (RCC(0x00)[10] == 0) {}           // wait for HSIRDY
        RCC(0x0C) = (1<<24) | (10<<8) | (2<<0); // 160 MHz w/ HSI
        RCC(0x00)[24] = 1;                      // PLLON
        while (RCC(0x00)[25] == 0) {}           // wait for PLLRDY
        RCC(0x08) = 0b11;                       // PLL as SYSCLK
    }

    void enableClkMaxMSI () { // using internal 48 MHz MSI
        FLASH(0x00) = 0x702;            // flash ACR, 2 wait states
        RCC(0x00)[0] = 1;               // MSION
        while (RCC(0x00)[1] == 0) {}    // wait for MSIRDY
        RCC(0x00).mask(3, 5) = 0b10111; // MSI 48 MHz
        RCC(0x08) = 0b00;               // MSI as SYSCLK
    }

    void enableClkSaver (int range) { // using MSI at 100 kHz to 4 MHz
        RCC(0x00)[0] = 1;                // MSION
        while (RCC(0x00)[1] == 0) {}     // wait for MSIRDY
        RCC(0x08) = 0b00;                // MSI as SYSCLK
        RCC(0x00) = (range<<4) | (1<<3); // MSI 48 MHz, ~HSION, ~PLLON
        FLASH(0x00) = 0;                 // no ACR, no wait states
    }

    auto fastClock (bool pll) -> uint32_t {
        PWR(0x00).mask(9, 2) = 0b01;    // VOS range 1
        while (PWR(0x14)[10] != 0) {}   // wait for ~VOSF
        if (pll) enableClkWithPLL(); else enableClkMaxMSI();
        return SystemCoreClock = pll ? 80000000 : 48000000;
    }
    auto slowClock (bool low) -> uint32_t {
        enableClkSaver(low ? 0b0000 : 0b0110);
        PWR(0x00).mask(9, 2) = 0b10;    // VOS range 2
        return SystemCoreClock = low ? 100000 : 4000000;
    }
#endif

    void powerDown (bool standby) {
        switch (FAMILY) {
            case STM_F4:
            case STM_F7:
                RCC(0x40)[28] = 1; // PWREN
                PWR(0x00)[1] = standby; // PDDS if standby
                break;
            case STM_L4:
                RCC(0x58)[28] = 1; // PWREN
                // set to either shutdown or stop 1 mode
                PWR(0x00) = (0b01<<9) | (standby ? 0b100 : 0b001);
                PWR(0x18) = 0b1'1111; // clear CWUFx
                break;
        }
        SCB(0xD10)[2] = 1; // set SLEEPDEEP
        asm ("wfe");
    }

    void dwt::start () {
        DWT(LAR) = 0xC5ACCE55;
        SCB(DEMCR) = SCB(DEMCR) | (1<<24); // set TRCENA in DEMCR
        clear();
        DWT(CTRL) = DWT(CTRL) | 1;
    }
    void dwt::stop () {
        DWT(CTRL) = DWT(CTRL) & ~1;
    }

    extern "C" void irqDispatch () {
        uint8_t irq = SCB(0xD04); // ICSR
        auto idx = Device::irqMap[irq-16];
        Device::devMap[idx]->irqHandler();
    }
}

// to re-generate "irqs.h", see the "all-irqs.sh" script

#define IRQ(f) void f () __attribute__ ((alias ("irqDispatch")));
extern "C" {
#include "irqs.h"
}
