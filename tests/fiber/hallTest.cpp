#include "hall.h"
#include "doctest.h"

using namespace hall;
using namespace boss;
using systick::millis;

void boss::debugf (const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

TEST_CASE("systick") {
    systick::init();

    SUBCASE("ticking") {
        auto t = millis();
        idle();
        CHECK(t+1 == millis());
        idle();
        CHECK(t+2 == millis());
    }

    SUBCASE("micros same") {
        auto t1 = systick::micros();
        auto t2 = systick::micros();
        auto t3 = systick::micros();
        // might still fail, but the chance of that should be almost zero
        CHECK(t1 <= t3);
        CHECK(t3 - t1 <= 1);
        CHECK((t1-t2) * (t2-t3) == 0); // one of those must be identical
    }

    SUBCASE("micros changed") {
        auto t = systick::micros();
        for (volatile int i = 0; i < 1000; ++i) {}
        CHECK(t != systick::micros());
    }

    systick::deinit();
}

TEST_CASE("pool") {
    buffers.init();

    auto p = buffers.allocate(), q = buffers.allocate(), r = buffers.allocate();

    SUBCASE("allocate") {
        CHECK(buffers.idOf(p) == 1);
        CHECK(buffers.idOf(q) == 2);
        CHECK(buffers.idOf(r) == 3);

        CHECK(q >= p + Pool::BUFLEN);
        CHECK(r >= q + Pool::BUFLEN);

        CHECK(p == buffers[1]);
        CHECK(q == buffers[2]);
        CHECK(r == buffers[3]);
    }

    SUBCASE("lifo reuse") {
        buffers.release(3);
        buffers.releasePtr(q);
        CHECK(buffers.allocate() == q);
        CHECK(buffers.allocate() == r);
        CHECK(buffers.allocate() > r);
    }

    SUBCASE("allocate all") {
        auto nFree = buffers.items(0) - 1; // the free list is not a queue!
        CAPTURE(nFree);
        CHECK(nFree >= 3);
        CHECK(buffers.hasFree() == true);

        for (int i = 0; i < nFree-1; ++i)
            CHECK(buffers.allocate() != nullptr);
        CHECK(buffers.hasFree());

        CHECK(buffers.allocate() != nullptr);
        CHECK(buffers.hasFree() == false);
    }

    SUBCASE("release") {
        auto nItems = buffers.items(0);
        buffers.releasePtr(p);
        buffers.releasePtr(q);
        buffers.releasePtr(r);
        CHECK(buffers.items(0) == nItems + 3);
    }

    SUBCASE("re-init") {
        auto nItems = buffers.items(0);
        buffers.init();
        CHECK(buffers.items(0) == nItems + 3);
    }

    buffers.check();
}

TEST_CASE("fiber") {
    systick::init(1);
    buffers.init();

    SUBCASE("no fibers") {
        auto nItems = buffers.items(0);
        while (Fiber::runLoop()) {}
        CHECK(buffers.items(0) == nItems);
    }

    SUBCASE("single timer") {
        auto t = millis();
        auto nItems = buffers.items(0);
        CHECK(Fiber::ready.isEmpty());

        Fiber::create([](void*) {
            for (int i = 0; i < 3; ++i)
                Fiber::msWait(10);
        });

        CHECK(buffers.items(0) == nItems - 1);
        CHECK(Fiber::ready.isEmpty() == false);

        auto busy = true;
        for (int i = 0; i < 50; ++i) {
            busy = Fiber::runLoop();
            CHECK(Fiber::ready.isEmpty());
            if (!busy)
                break;
            CHECK(buffers.items(0) == nItems - 1);
            CHECK(!Fiber::timers.isEmpty());
            idle();
        }

        CHECK(!busy);
        CHECK(buffers.items(0) == nItems);
        CHECK(millis() - t == 30);
    }

    SUBCASE("multiple timers") {
        static uint32_t t, n; // static so fiber lambda can use it w/o capture
        t = millis();

        auto nItems = buffers.items(0);
        CHECK(Fiber::ready.isEmpty());

        constexpr auto N = 3;
        constexpr auto S = "987";
        for (int i = 0; i < N; ++i)
            Fiber::create([](void* p) {
                uint8_t ms = (uintptr_t) p;
                for (int i = 0; i < 5; ++i) {
                    Fiber::msWait(ms);
                    //printf("ms  %d now %d\n", ms, millis() - t);
                    if ((millis() - t) % ms == 0)
                        ++n;
                }
            }, (void*)(uintptr_t) (S[i]-'0'));

        CHECK(buffers.items(0) == nItems - N);
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
            CHECK(buffers.items(0) == nItems);
            CHECK(millis() - t == 45);
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
