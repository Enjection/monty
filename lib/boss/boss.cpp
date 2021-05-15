#include "hall.h"
#include <cstring>

using namespace hall;
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

static uint32_t bufferSpace [20 * 192 / 4];
Pool boss::pool (bufferSpace, sizeof bufferSpace);

Pool::Pool (void* ptr, size_t len)
        : nBuf (len / sizeof (Buffer)), bufs ((Buffer*) ptr) {
    assert(len / sizeof (Buffer) <= 256);    //buffer id must fit in a uint8_t
    assert(nBuf <= BUFLEN);  //free chain must fit in buffer[0]
    assert(((uintptr_t) ptr % 4) == 0);
    init();
}

void Pool::init () {
    for (int i = 0; i < nBuf-1; ++i)
        tag(i) = i+1;
    tag(nBuf-1) = 0;
}

auto Pool::allocate () -> uint8_t* {
    auto n = tag(0);
    assert(n != 0);
    tag(0) = tag(n);
    tag(n) = 0;
    return bufs[n].b;
}

auto Pool::items (uint8_t i) const -> int {
    int n = 0;
    do {
        ++n;
        i = tag(i);
    } while (i != 0);
    return n;
}

void Pool::check () const {
    bool inUse [nBuf];
    for (int i = 0; i < nBuf; ++i)
        inUse[i] = 0;
    for (int i = tag(0); i != 0; i = tag(i)) {
        assert(!inUse[i]);
        inUse[i] = true;
    }
}

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
        uint16_t remain = f.timeout - now;
debugf("FQe n %d r %d t %d n %d\n", *p, remain, f.timeout, now);
        if (remain == 0 || remain > 60'000) {
            auto c = *p;
            if (last == c)
                last = next;
            *p = next;
            ++num;
            Fiber::resume(c, 0);
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
    if (bottom == nullptr && setjmp(returner) == 0)
        bottom = &dummy;
    processPending();
    curr = ready.pull();
    if (curr != 0) {
        auto& fp = at(curr);
        if (fp.stat != -128)
            longjmp(fp.context, 1);
        fp.fun(fp.arg);
    }
    return true;
}

auto Fiber::create (void (*fun)(void*), void* arg) -> Fid_t {
    systick::expirer = [](uint16_t now, uint16_t& limit) {
debugf("se+ n %d l %d\n", now, limit);
        timers.expire(now, limit);
debugf("se- n %d l %d\n", now, limit);
    };

    auto fp = (Fiber*) pool.allocate();
    fp->timeout = (uint16_t) systick::millis(); // TODO needed?
    fp->stat = -128; // mark as not-started
    fp->fun = fun;
    fp->arg = arg;
    auto id = pool.idOf(fp);
    ready.append(id);
    return id;
}

auto resumeFixer (void* top) {
    auto fp = &Fiber::at(Fiber::curr);
    memcpy(top, fp->data, (uintptr_t) bottom - (uintptr_t) top);
    return fp->stat;
}

void Fiber::processPending () {
    if (Device::dispatch()) {
        uint16_t limit = 100; // TODO arbitrary? also: 24-bit limit on ARM
        timers.expire(systick::millis(), limit);
        systick::init(limit);
    }
}

auto Fiber::suspend (Queue& q, uint16_t ms) -> int {
    auto fp = curr == 0 ? (Fiber*) pool.allocate() : &at(curr);
    curr = 0;
    fp->timeout = (uint16_t) systick::millis() + ms;
    fp->stat = 0;
    q.append(pool.idOf(fp));
    if (setjmp(fp->context) == 0) {
#if 0
        debugf("\tFS %d emp %d top %p data %p bytes %d\n",
                pool.idOf(fp), ready.isEmpty(), &fp, fp->data,
                (int) ((uintptr_t) bottom - (uintptr_t) &fp));
#endif
        memcpy(fp->data, &fp, (uintptr_t) bottom - (uintptr_t) &fp);
        curr = 0;
        longjmp(returner, 1);
    }
    return resumeFixer(&fp);
}

void Fiber::duff (uint32_t* dst, uint32_t const* src, uint32_t cnt) {
    // see https://en.wikipedia.org/wiki/Duff%27s_device
    auto n = (cnt + 7) / 8;
    switch (cnt % 8) {
        case 0: do { *dst++ = *src++; [[fallthrough]];
        case 7:      *dst++ = *src++; [[fallthrough]];
        case 6:      *dst++ = *src++; [[fallthrough]];
        case 5:      *dst++ = *src++; [[fallthrough]];
        case 4:      *dst++ = *src++; [[fallthrough]];
        case 3:      *dst++ = *src++; [[fallthrough]];
        case 2:      *dst++ = *src++; [[fallthrough]];
        case 1:      *dst++ = *src++;
                } while (--n > 0);
    }
}

void Semaphore::expire (uint16_t now, uint16_t& limit) {
    if (count < 0)
        count += Queue::expire(now, limit);
}
