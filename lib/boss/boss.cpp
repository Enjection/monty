#include "boss.h"

auto boss::veprintf(void (*fun)(void*,int), void* arg,
                    char const* fmt, va_list ap) -> int {
    int pad, count = 0;
    auto emit = [&](int c) { ++count; fun(arg, c); };
    auto fill = [&](int n) { while (--n >= 0) emit(pad); };

    while (*fmt)
        if (char c = *fmt++; c != '%')
            emit(c);
        else {
            pad = *fmt == '0' ? '0' : ' ';
            int width = 0, radix = 0;
            while (radix == 0)
                switch (c = *fmt++) {
                    case 'b': radix =  2; break;
                    case 'o': radix =  8; break;
                    case 'u':
                    case 'd': radix = 10; break;
                    case 'p': pad = '0'; width = 8;
                                [[fallthrough]];
                    case 'x': radix = 16; break;
                    case 'c': fill(width - 1);
                                c = va_arg(ap, int);
                                [[fallthrough]];
                    case '%': emit(c); radix = 1; break;
                    case '*': width = va_arg(ap, int); break;
                    case 's': { char const* s = va_arg(ap, char const*);
                                while (*s) {
                                    emit(*s++);
                                    --width;
                                }
                                fill(width);
                                }
                                [[fallthrough]];
                    default:  if ('0' <= c && c <= '9')
                                width = 10 * width + c - '0';
                                else
                                radix = 1; // stop scanning
                }
            if (radix > 1) {
                static uint8_t buf [32]; // shared by all printf's
                uint8_t pos = 0;
                int val = va_arg(ap, int);
                auto sign = val < 0 && c == 'd';
                uint32_t num = sign ? -val : val;
                do {
                    buf[pos++] = "0123456789ABCDEF"[num % radix];
                    num /= radix;
                } while (num != 0);
                if (sign) {
                    if (pad == ' ')
                        buf[pos++] = '-';
                    else {
                        --width;
                        emit('-');
                    }
                }
                fill(width - pos);
                while (pos > 0)
                    emit(buf[--pos]);
            }
        }

    return count;
}
