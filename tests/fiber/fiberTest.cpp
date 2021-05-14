#include "hall.h"
#include "doctest.h"

using namespace hall;
using namespace boss;

TEST_CASE("systick") {
    systick::init();

    SUBCASE("ticking") {
        auto t = systick::millis();
        idle();
        CHECK(t+1 == systick::millis());
        idle();
        CHECK(t+2 == systick::millis());
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

TEST_CASE("fiber") {
    systick::init();

    SUBCASE("no fibers") {
        //while (Fiber::runLoop()) {}
    }

    SUBCASE("one fiber") {
        Fiber::create([](void*) {
            while (true) {
//puts("11!");
                Fiber::msWait(1000);
//puts("22!");
                printf("%u\n", systick::millis());
            }
        });

        auto busy = true;
        for (int i = 0; busy && i < 10'000'000; ++i)
            busy = Fiber::runLoop();

        //TEST_ASSERT_FALSE(busy);
        //while (Fiber::runLoop()) {}
    }

    systick::deinit();
}
