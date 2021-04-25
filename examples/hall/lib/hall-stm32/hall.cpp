#include "hall.h"
#include <cstring>

namespace hall {
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

    auto Pin::define (char const* desc) -> int {
        if ('A' <= *desc && *desc <= 'O') {
            _port = *desc++ - 'A';
            _pin = 0;
            while ('0' <= *desc && *desc <= '9')
                _pin = 10 * _pin + *desc++ - '0';
        }
        return _port == 15 ? -1 : *desc++ != ':' ? 0 : mode(desc);
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
}
