#include "boss.h"
#include <cstring>

namespace boss {
    auto veprintf(void (*fun) (void*,int), void* arg,
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

    void debugf (const char* fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        veprintf(debugPutc, nullptr, fmt, ap);
        va_end(ap);
    }

    void failAt (void const* pc, void const* lr) { // weak, can be redefined
        debugf("failAt %p %p\n", pc, lr);
        for (uint32_t i = 0; i < systemHz() >> 15; ++i) {}
        systemReset();
    }

    Pool<> pool;

    auto Queue::pull () -> uint8_t {
        auto i = first;
        if (i != 0) {
            first = pool.tag(i);
            if (first == 0)
                last = 0;
            pool.tag(i) = 0;
        }
        return i;
    }

    void Queue::insert (uint8_t i) {
        pool.tag(i) = first;
        first = i;
        if (last == 0)
            last = first;
    }

    void Queue::append (uint8_t i) {
        if (last != 0)
            pool.tag(last) = i;
        last = i;
        if (first == 0)
            first = last;
    }

    uint8_t Fiber::curr;
    Queue Fiber::ready;
    jmp_buf* resumer;

    void Fiber::runLoop () {
debugf(" R>");
        jmp_buf bottom;
        resumer = &bottom;
        setjmp(bottom);
debugf(" S>");
        Device::processAllPending();
        curr = ready.pull();
        if (curr != 0) {
debugf(" L>");
            longjmp(at(curr)._context, 1);
        }
debugf(" I>");
    }

    static void resumeFixer (void* top) {
        auto fp = &Fiber::at(Fiber::curr);
        debugf("\tRF %d top %p data %p resumer %p\n",
                Fiber::curr, top, fp->_data, resumer);
        memcpy(top, fp->_data, (uintptr_t) resumer - (uintptr_t) top);
debugf(" X>");
    }

    void Fiber::suspend (Queue& q, uint16_t) {
debugf(" E>");
        auto fp = curr == 0 ? new (pool.allocate()) Fiber : &at(curr);
        q.append(fp->id());
        if (setjmp(fp->_context) == 0) {
            debugf("\tFS %d top %p data %p resumer %p\n",
                    fp->id(), &fp, fp->_data, resumer);
            memcpy(fp->_data, &fp, (uintptr_t) resumer - (uintptr_t) &fp);
            longjmp(*resumer, 1);
        }
debugf(" F>");
        resumeFixer(&fp);
    }
}
