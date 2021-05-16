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

static uint32_t bufferSpace [25 * 192 / 4];
Pool boss::buffers (bufferSpace, sizeof bufferSpace);

Pool::Pool (void* ptr, size_t len)
        : nBuf (len / sizeof (Buffer)), bufs ((Buffer*) ptr) {
    assert(len / sizeof (Buffer) <= 256); // buffer id must fit in a uint8_t
    assert(nBuf <= BUFLEN); // free chain must fit in buffer[0]
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

// resume all fibers whose time has come and return that count
// also reduce the time limit to the earliest timeout coming up
auto Fiber::expire (pool::Queue& q, uint16_t now, uint16_t& limit) -> int {
    auto expired = q.remove([&](int id) {
        auto& f = Fiber::at(id);
        uint16_t remain = f.timeout - now;
        if (remain == 0 || remain > 60'000)
            return true;
        if (limit > remain)
            limit = remain;
        return false;
    });
    for (int num = 0; true; ++num) {
        auto id = expired.pull();
        if (id == 0)
            return num;
        auto& f = Fiber::at(id);
        f.timeout = now + 60'000;
        f.resume(0);
    }
}

Fid_t Fiber::curr;
pool::Queue Fiber::ready;
pool::Queue Fiber::timers;

jmp_buf returner;
uint32_t* bottom;

auto Fiber::runLoop () -> bool {
    uint32_t dummy;
    if (bottom == nullptr && setjmp(returner) == 0)
        bottom = &dummy;
    processPending();
    curr = ready.pull();
    if (curr != 0) {
        auto& f = at(curr);
        if (f.result != -128)
            longjmp(f.context, 1);
        f.fun(f.arg);
        buffers.release(f.id());
    }
    return !timers.isEmpty();
}

auto Fiber::create (void (*fun)(void*), void* arg) -> Fid_t {
    if (systick::expirer == nullptr)
        systick::expirer = [](uint16_t now, uint16_t& limit) {
            expire(timers, now, limit);
        };

    auto fp = (Fiber*) buffers.allocate();
    fp->timeout = (uint16_t) systick::millis() + 60'000; // TODO needed?
    fp->result = -128; // mark as not-started
    fp->fun = fun;
    fp->arg = arg;
    auto id = buffers.idOf(fp);
    ready.append(id);
    return id;
}

auto resumeFixer (void* top) {
    auto fp = &Fiber::at(Fiber::curr);
    memcpy(top, fp->data, (uintptr_t) bottom - (uintptr_t) top);
    return fp->result;
}

void Fiber::processPending () {
    if (Device::dispatch()) {
        uint16_t limit = 100; // TODO arbitrary? also: 24-bit limit on ARM
        expire(timers, systick::millis(), limit);
        systick::init(1);
    }
}

auto Fiber::suspend (pool::Queue& q, uint16_t ms) -> int {
    auto fp = curr == 0 ? (Fiber*) buffers.allocate() : &at(curr);
    curr = 0;
    fp->timeout = (uint16_t) systick::millis() + ms;
    fp->result = 0;
    q.append(buffers.idOf(fp));
    if (setjmp(fp->context) == 0) {
#if 0
        debugf("\tFS %d emp %d top %p data %p bytes %d\n",
                buffers.idOf(fp), ready.isEmpty(), &fp, fp->data,
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
        count += Fiber::expire(*this, now, limit);
}
