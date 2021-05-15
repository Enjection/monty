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
    pool.init();

    auto p = pool.allocate(), q = pool.allocate(), r = pool.allocate();

    SUBCASE("allocate") {
        CHECK(pool.idOf(p) == 1);
        CHECK(pool.idOf(q) == 2);
        CHECK(pool.idOf(r) == 3);

        CHECK(q >= p + Pool::BUFLEN);
        CHECK(r >= q + Pool::BUFLEN);

        CHECK(p == pool[1]);
        CHECK(q == pool[2]);
        CHECK(r == pool[3]);
    }

    SUBCASE("lifo reuse") {
        pool.release(3);
        pool.releasePtr(q);
        CHECK(pool.allocate() == q);
        CHECK(pool.allocate() == r);
        CHECK(pool.allocate() > r);
    }

    SUBCASE("allocate all") {
        auto nFree = pool.items(0) - 1; // the free list is not a queue!
        CAPTURE(nFree);
        CHECK(nFree >= 3);
        CHECK(pool.hasFree() == true);

        for (int i = 0; i < nFree-1; ++i)
            CHECK(pool.allocate() != nullptr);
        CHECK(pool.hasFree());

        CHECK(pool.allocate() != nullptr);
        CHECK(pool.hasFree() == false);
    }

    SUBCASE("release") {
        auto nItems = pool.items(0);
        pool.releasePtr(p);
        pool.releasePtr(q);
        pool.releasePtr(r);
        CHECK(pool.items(0) == nItems + 3);
    }

    SUBCASE("re-init") {
        auto nItems = pool.items(0);
        pool.init();
        CHECK(pool.items(0) == nItems + 3);
    }

    pool.check();
}

TEST_CASE("fiber") {
    systick::init(1);
    pool.init();

    SUBCASE("no fibers") {
        auto nItems = pool.items(0);
        while (Fiber::runLoop()) {}
        CHECK(pool.items(0) == nItems);
    }

    SUBCASE("single timer") {
        auto t = millis();
        auto nItems = pool.items(0);
        CHECK(Fiber::ready.isEmpty());

        Fiber::create([](void*) {
            for (int i = 0; i < 3; ++i)
                Fiber::msWait(10);
        });

        CHECK(pool.items(0) == nItems - 1);
        CHECK(Fiber::ready.isEmpty() == false);

        auto busy = true;
        for (int i = 0; i < 50; ++i) {
            busy = Fiber::runLoop();
            CHECK(Fiber::ready.isEmpty());
            if (!busy)
                break;
            CHECK(pool.items(0) == nItems - 1);
            CHECK(!Fiber::timers.isEmpty());
            idle();
        }

        CHECK(!busy);
        CHECK(pool.items(0) == nItems);
        CHECK(millis() - t >= 30);
        CHECK(millis() - t < 35);
    }

    SUBCASE("multiple timers") {
        auto t = millis();
        auto nItems = pool.items(0);
        CHECK(Fiber::ready.isEmpty());

        constexpr auto N = 3; // FIXME fails with N set to 2 or 3
        constexpr auto S = "987";
        for (int i = 0; i < N; ++i)
            Fiber::create([](void* p) {
                uint8_t ms = (uintptr_t) p;
                printf("ms+ %d now %d\n", ms, millis());
                for (int i = 0; i < 5; ++i) {
                    Fiber::msWait(ms);
                    printf("ms  %d now %d\n", ms, millis());
                }
            }, (void*)(uintptr_t) (S[i]-'0'));

        CHECK(pool.items(0) == nItems - N);
        CHECK(Fiber::ready.isEmpty() == false);

        auto busy = true;
        for (int i = 0; busy && i < 100; ++i) {
            busy = Fiber::runLoop();
            CHECK(Fiber::ready.isEmpty());
            if (!busy)
                break;
            CHECK(pool.items(0) == nItems - N);
            CHECK(Fiber::timers.isEmpty() == !busy);
            idle();
        }

        CHECK(!busy);
        CHECK(millis() - t > 40);
        CHECK(millis() - t < 50);
    }

    systick::deinit();
}
