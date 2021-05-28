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

namespace boss::pool {
    Buf* buffers;
    uint8_t numBufs;
    uint8_t numFree;
    Queue freeBufs;

    void init (void* ptr, size_t len) {
        buffers = (Buf*) ptr;
        numFree = numBufs = len / sizeof (Buf) - 1;
        for (int i = 1; i <= numBufs; ++i)
            freeBufs.append(i);
    }

    void deinit () {
        buffers = nullptr;
        numBufs = numFree = 0;
        freeBufs = {};
    }

    auto Buf::operator new (size_t bytes) -> void* {
        (void) bytes; // TODO check that it fits
        auto id = freeBufs.pull();
        //TODO check != 0
        --numFree;
        return &Buf::at(id);
    }

    void Buf::operator delete (void* p) {
        freeBufs.insert((Buf*) p);
        ++numFree;
    }

    auto Buf::tag () -> Id_t& {
        return buffers->data[this - buffers];
    }

    auto Buf::asId (void const* p) -> Id_t {
        return (Buf const*) p - buffers;
    }

    auto Buf::at (Id_t id) -> Buf& {
        return buffers[id];
    }

    auto Queue::pull () -> Id_t {
        auto id = head;
        if (id != 0) {
            head = tag(id);
            if (head == 0)
                tail = 0;
            tag(id) = 0;
        }
        return id;
    }

    void Queue::insert (Id_t id) {
        tag(id) = head;
        head = id;
        if (tail == 0)
            tail = head;
    }

    void Queue::append (Id_t id) {
        tag(id) = 0;
        if (tail != 0)
            tag(tail) = id;
        tail = id;
        if (head == 0)
            head = tail;
    }
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

Fiber::Fid_t Fiber::curr;
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
        delete (pool::Buf*) &f;
    }
    return pool::numFree < pool::numBufs;
}

auto Fiber::create (void (*fun)(void*), void* arg) -> Fid_t {
    if (systick::expirer == nullptr)
        systick::expirer = [](uint16_t now, uint16_t& limit) {
            expire(timers, now, limit);
        };

    auto fp = (Fiber*) new pool::Buf;
    fp->timeout = (uint16_t) systick::millis() + 60'000; // TODO needed?
    fp->result = -128; // mark as not-started
    fp->fun = fun;
    fp->arg = arg;
    ready.append(fp);
    return fp->id();
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
    auto fp = curr == 0 ? (Fiber*) new pool::Buf : &at(curr);
    curr = 0;
    fp->timeout = (uint16_t) systick::millis() + ms;
    fp->result = 0;
    q.append(fp);
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

#if DOCTEST
#include <doctest.h>

void boss::debugf (const char* fmt, ...) __attribute__ ((weak)) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

namespace boss::pool {
    doctest::String toString(Buf* p) {
        char buf [20];
        sprintf(buf, "Buf(%d)", (int) (p - buffers));
        return buf;
    }
}

using namespace boss;
using namespace pool;

// only implemented in test builds
void Queue::verify () const {
    if (head == 0 && tail == 0)
        return;
    CAPTURE((int) numBufs);
    Id_t tags [numBufs+1];
    memset(tags, 0, sizeof tags);
    for (auto curr = head; curr != 0; curr = tag(curr)) {
        CAPTURE((int) curr);
        CHECK(tags[curr] == 0);
        tags[curr] = 1;

        auto next = tag(curr);
        if (next == 0)
            CHECK(curr == tail);
        else
            CHECK(curr != tail);
    }
}

TEST_CASE("buffer") {
    uint8_t mem [5000];
    init(mem, sizeof mem);

    INFO("numBufs: ", (int) numBufs);
    CHECK(numFree == numBufs);

    auto bp = new Buf;
    CHECK(bp != nullptr);
    CHECK(bp->id() != 0);

    Queue q;
    CHECK(q.isEmpty());

    CHECK(q.pull() == 0);

    q.append(bp);
    CHECK(!q.isEmpty());

    auto i = q.pull();
    CHECK(i != 0);
    CHECK(q.isEmpty());

    auto& r = Buf::at(i);
    CHECK(&r == bp);

    freeBufs.verify();
    deinit();
}

TEST_CASE("pool") {
    uint8_t mem [5000];
    init(mem, sizeof mem);

    SUBCASE("new") {
        auto bp = new Buf;
        auto i = bp->id();
        CHECK(i == 1);
        CHECK(&Buf::at(i) == bp);

        auto bp2 = new Buf;
        CHECK(bp2->id() != 0);
        CHECK(bp2 != bp);
    }

    SUBCASE("reuse deleted") {
        auto p = new Buf;
        delete p;
        CHECK(new Buf == p);
    }

    SUBCASE("reuse in lifo order") {
        auto p = new Buf, q = new Buf, r = new Buf;
        delete p;
        delete q;
        delete r;
        CHECK(new Buf == r);
        CHECK(new Buf == q);
        CHECK(new Buf == p);
    }

    freeBufs.verify();
    deinit();
}

TEST_CASE("queue") {
    uint8_t mem [5000];
    init(mem, sizeof mem);

    Queue q;

    SUBCASE("insert one") {
        q.insert(new Buf);
        CHECK(q.pull() == 1);
        CHECK(q.isEmpty());
    }

    SUBCASE("insert many") {
        q.insert(new Buf);
        q.insert(new Buf);
        q.insert(new Buf);
        CHECK(q.pull() == 3);
        CHECK(q.pull() == 2);
        CHECK(q.pull() == 1);
        CHECK(q.isEmpty());
    }

    SUBCASE("append one") {
        q.append(new Buf);
        CHECK(q.pull() == 1);
        CHECK(q.isEmpty());
    }

    SUBCASE("append many") {
        q.append(new Buf);
        q.append(new Buf);
        q.append(new Buf);
        CHECK(q.pull() == 1);
        CHECK(q.pull() == 2);
        CHECK(q.pull() == 3);
        CHECK(q.isEmpty());
    }

    SUBCASE("remove") {
        for (int i = 0; i < 5; ++i) {
            auto p = new Buf;
            CHECK(p->id() == i + 1);
            q.append(p);
        }

        SUBCASE("none") {
            q.remove([](int) { return false; });
            q.verify();
            for (int i = 0; i < 5; ++i)
                CHECK(q.pull() == i + 1);
            CHECK(q.isEmpty());
        }

        SUBCASE("first") {
            q.remove([](int id) { return id == 1; });
            q.verify();
            CHECK(q.pull() == 2);
            CHECK(q.pull() == 3);
            CHECK(q.pull() == 4);
            CHECK(q.pull() == 5);
            CHECK(q.isEmpty());
        }

        SUBCASE("last") {
            q.remove([](int id) { return id == 5; });
            q.verify();
            CHECK(q.pull() == 1);
            CHECK(q.pull() == 2);
            CHECK(q.pull() == 3);
            CHECK(q.pull() == 4);
            CHECK(q.isEmpty());
        }

        SUBCASE("even") {
            q.remove([](int id) { return id % 2 == 0; });
            q.verify();
            CHECK(q.pull() == 1);
            CHECK(q.pull() == 3);
            CHECK(q.pull() == 5);
            CHECK(q.isEmpty());
        }

        SUBCASE("all") {
            q.remove([](int) { return true; });
            q.verify();
            CHECK(q.isEmpty());
        }
    }

    freeBufs.verify();
    deinit();
}

TEST_CASE("fiber") {
    uint8_t mem [5000];
    pool::init(mem, sizeof mem);

    systick::init(1);

    SUBCASE("no fibers") {
        auto nItems = numFree;
        while (Fiber::runLoop()) {}
        CHECK(numFree == nItems);
    }

    SUBCASE("single timer") {
        auto t = systick::millis();
        auto nItems = numFree;
        CHECK(Fiber::ready.isEmpty());

        Fiber::create([](void*) {
            for (int i = 0; i < 3; ++i)
                Fiber::msWait(10);
        });

        CHECK(numFree == nItems - 1);
        CHECK(Fiber::ready.isEmpty() == false);

        auto busy = true;
        for (int i = 0; i < 50; ++i) {
            busy = Fiber::runLoop();
            CHECK(Fiber::ready.isEmpty());
            if (!busy)
                break;
            CHECK(numFree == nItems - 1);
            CHECK(!Fiber::timers.isEmpty());
            idle();
        }

        CHECK(!busy);
        CHECK(numFree == nItems);
        CHECK(systick::millis() - t == 30);
    }

    SUBCASE("multiple timers") {
        static uint32_t t, n; // static so fiber lambda can use it w/o capture
        t = systick::millis();

        auto nItems = numFree;
        CHECK(Fiber::ready.isEmpty());

        constexpr auto N = 3;
        constexpr auto S = "987";
        for (int i = 0; i < N; ++i)
            Fiber::create([](void* p) {
                uint8_t ms = (uintptr_t) p;
                for (int i = 0; i < 5; ++i) {
                    Fiber::msWait(ms);
                    //printf("ms  %d now %d\n", ms, systick::millis() - t);
                    if ((systick::millis() - t) % ms == 0)
                        ++n;
                }
            }, (void*)(uintptr_t) (S[i]-'0'));

        CHECK(numFree == nItems - N);
        CHECK(Fiber::ready.isEmpty() == false);

        SUBCASE("run") {
            auto busy = true;
            for (int i = 0; i < 100; ++i) {
                busy = Fiber::runLoop();
                CHECK(Fiber::ready.isEmpty());
                if (!busy)
                    break;
                CHECK(!Fiber::timers.isEmpty());
                idle();
            }

            CHECK(n == 3 * 5);
            CHECK(!busy);
            CHECK(numFree == nItems);
            CHECK(systick::millis() - t == 45);
            CHECK(Fiber::ready.isEmpty());
            CHECK(Fiber::timers.isEmpty());
        }

        SUBCASE("dump") {
            CHECK(!Fiber::ready.isEmpty());
            CHECK(Fiber::timers.isEmpty());
        }
    }

    systick::deinit();
}

#endif // DOCTEST
