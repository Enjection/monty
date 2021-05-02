#include "boss.h"
#include <cstring>

namespace boss {
    auto veprintf(void (*fun)(void*,int), void* arg,
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
        pool.tag(i) = 0;
        if (last != 0)
            pool.tag(last) = i;
        last = i;
        if (first == 0)
            first = last;
    }

    auto Queue::expire (uint16_t now, uint16_t& limit) -> int {
        int num = 0;
        for (uint8_t* p = &first; *p != 0; p = &pool.tag(*p)) {
            auto& f = Fiber::at(*p);
            uint16_t remain = f._timeout - now;
            if (remain == 0 || remain > 60'000) {
                auto next = pool.tag(*p);
                if (last == *p)
                    last = next;
                Fiber::resume(*p, 0);
                ++num;
                *p = next;
            } else if (limit > remain)
{ //if (remain > 2000) debugf("s %p %d\n", &f, remain);
                limit = remain;
}
        }
        return num;
    }

    uint8_t Fiber::curr;
    Queue Fiber::ready;
    void (*Fiber::app)() = []{};
    jmp_buf returner;
    uint32_t* bottom;

    void Fiber::runLoop () {
        //debugf(" R>");
        uint32_t dummy;
        if (bottom == nullptr && setjmp(returner) == 0) {
            bottom = &dummy;
            //debugf(" A>");
            app();
        }
        //debugf(" S>");
        Device::processAllPending();
        curr = ready.pull();
        if (curr != 0) {
            //debugf(" L>");
            longjmp(at(curr)._context, 1);
        }
        //debugf(" I>");
    }

    extern void blah () { debugf(""); }

    static auto resumeFixer (void* top) {
        auto fp = &Fiber::at(Fiber::curr);
#if 0
        debugf("\tRF %d top %p data %p bottom %p\n",
                Fiber::curr, top, fp->_data, bottom);
#endif
        memcpy(top, fp->_data, (uintptr_t) bottom - (uintptr_t) top);
        //debugf(" X>");
asm ("nop");
        return fp->_status;
    }

    auto Fiber::suspend (Queue& q, uint16_t ms) -> int {
        //debugf(" E>");
        auto fp = curr == 0 ? new (pool.allocate()) Fiber : &at(curr);
        curr = 0;
        fp->_timeout = (uint16_t) systick::millis() + ms;
        q.append(fp->id());
        if (setjmp(fp->_context) == 0) {
#if 0
            debugf("\tFS %d top %p data %p bottom %p\n",
                    fp->id(), &fp, fp->_data, bottom);
#endif
            memcpy(fp->_data, &fp, (uintptr_t) bottom - (uintptr_t) &fp);
            longjmp(returner, 1);
        }
        //debugf(" F>");
        return resumeFixer(&fp);
    }

    void Semaphore::expire (uint16_t now, uint16_t& limit) {
//debugf("f %d c %d n %d\n", queue.first, count, now);
        if (count < 0)
            count += queue.expire(now, limit);
//debugf(" l %d f %d\n", limit, queue.first);
    }
}
