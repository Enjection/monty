#include "boss.h"
#include <cstring>

using namespace boss;

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

void boss::failAt (void const* pc, void const* lr) {
    debugf("failAt %p %p\n", pc, lr);
    while (true) {}
}

Pool<> boss::pool;

auto Fiber::Queue::pull () -> Fid_t {
    auto i = first;
    if (i != 0) {
        first = pool.tag(i);
        if (first == 0)
            last = 0;
        pool.tag(i) = 0;
    }
    return i;
}

void Fiber::Queue::insert (Fid_t i) {
    pool.tag(i) = first;
    first = i;
    if (last == 0)
        last = first;
}

void Fiber::Queue::append (Fid_t i) {
    pool.tag(i) = 0;
    if (last != 0)
        pool.tag(last) = i;
    last = i;
    if (first == 0)
        first = last;
}

auto Fiber::Queue::expire (uint16_t now, uint16_t& limit) -> int {
    int num = 0;
    Fid_t* p = &first;
    while (*p != 0) {
        auto& next = pool.tag(*p);
        auto& f = Fiber::at(*p);
        uint16_t remain = f._timeout - now;
        if (remain == 0 || remain > 60'000) {
            auto c = *p;
            if (last == c)
                last = next;
            *p = next;
            Fiber::resume(c, 0);
            ++num;
        } else if (limit > remain)
            limit = remain;
        p = &next;
    }
    return num;
}

Fiber::Fid_t Fiber::curr;
Fiber::Queue Fiber::ready;
Fiber::Queue Fiber::timers;

jmp_buf returner;
uint32_t* bottom;

auto Fiber::runLoop () -> bool {
    uint32_t dummy;
    if (bottom == nullptr && setjmp(returner) == 0) {
        bottom = &dummy;
        app();
    }
    processPending();
    curr = ready.pull();
    if (curr != 0)
        longjmp(at(curr)._context, 1);
    return true;
}

static auto resumeFixer (void* top) {
    auto fp = &Fiber::at(Fiber::curr);
    memcpy(top, fp->_data, (uintptr_t) bottom - (uintptr_t) top);
asm ("nop");
    return fp->_status;
}

auto Fiber::suspend (Queue& q, uint16_t ms) -> int {
    auto fp = curr == 0 ? new (pool.allocate()) Fiber : &at(curr);
    curr = 0;
    fp->_timeout = (uint16_t) systick::millis() + ms;
    q.append(fp->id());
    if (setjmp(fp->_context) == 0) {
#if 0
        debugf("\tFS %d rdy %d top %p data %p bytes %d\n",
                fp->id(), ready.length(), &fp, fp->_data,
                (int) ((uintptr_t) bottom - (uintptr_t) &fp));
#endif
        memcpy(fp->_data, &fp, (uintptr_t) bottom - (uintptr_t) &fp);
        curr = 0;
        longjmp(returner, 1);
    }
    return resumeFixer(&fp);
}

void Semaphore::expire (uint16_t now, uint16_t& limit) {
    if (count < 0)
        count += Queue::expire(now, limit);
}
