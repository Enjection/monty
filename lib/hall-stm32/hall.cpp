#include "hall.h"
#include <cstring>

using namespace hall;
using namespace hall::dev;

extern uint32_t SystemCoreClock; // CMSIS

auto hall::systemHz () -> uint32_t {
    return SystemCoreClock;
}

void hall::idle () {
    asm ("wfi");
}

auto Pin::mode (int m) const -> int {
    if constexpr (FAMILY == STM::F1) {
        RCC(0x18) |= (1<<port) | (1<<0); // enable GPIOx and AFIO clocks
        // FIXME wrong, mode bits have changed, and it changes return value
        if (m == 0b1000 || m == 0b1100) {
            gpio32(ODR)[pin] = m & 0b0100;
            m = 0b1000;
        }
        gpio32(0x00).mask(4*pin, 4) = m; // CRL/CRH
    } else {
        // enable GPIOx clock
        switch (FAMILY) {
            case STM::F1: break;
            case STM::F3: RCC(0x14)[port] = 1; break;
            case STM::F4:
            case STM::F7: RCC(0x30)[port] = 1; asm ("dsb"); break;
            case STM::G0: RCC(0x34)[port] = 1; break;
            case STM::H7: RCC(0xE0)[port] = 1; asm ("dsb"); break;
            case STM::L0: RCC(0x2C)[port] = 1; break;
            case STM::L4: RCC(0x4C)[port] = 1; break;
        }

        gpio32(0x00).mask(2*pin, 2) = m;      // MODER
        gpio32(0x04).mask(  pin, 1) = m >> 2; // TYPER
        gpio32(0x08).mask(2*pin, 2) = m >> 3; // OSPEEDR
        gpio32(0x0C).mask(2*pin, 2) = m >> 5; // PUPDR
        gpio32(0x20).mask(4*pin, 4) = m >> 8; // AFRL/AFRH
    }
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
        port = *desc++ - 'A';
        pin = 0;
        while ('0' <= *desc && *desc <= '9')
            pin = 10 * pin + *desc++ - '0';
    }
    return port == 15 ? -1 : *desc++ != ':' ? 0 : mode(desc);
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

namespace hall::systick {
    constexpr auto SYSTICK = io32<0xE000'E010>;

    volatile uint32_t ticks;
    uint8_t rate;

    void init (uint8_t ms) {
        if (rate > 0)
            ticks = millis();
        rate = ms;
        SYSTICK(0x4) = (ms*(systemHz()/1000))/8-1; // reload value
        SYSTICK(0x8) = 0;                          // current
        SYSTICK(0x0) = 0b011;                      // control, ÷8 mode
    }

    auto millis () -> uint32_t {
        while (true) {
            uint32_t t = ticks, n = SYSTICK(0x8);
            if (t == ticks)
                return t + rate - (n*8)/(systemHz()/1000);
        } // ticked just now, spin one more time
    }

    extern "C" void SysTick_Handler () {
        ticks += rate;
    }
}
