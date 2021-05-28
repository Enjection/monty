#include "hall.h"
#include <pthread.h>
#include <ctime>

using namespace hall;
using namespace boss;

namespace boss { void debugf (const char* fmt, ...); }
using boss::debugf;

void hall::idle () {
    auto t = systick::millis();
    while (t == systick::millis()) {
        timespec ts { 0, 100'000 };
        nanosleep(&ts, &ts); // 100 µs, i.e. 10% of ticks' 1 ms resolution
    }
}

void hall::systemReset () {
    exit(1);
}

Device* devMap [20];  // must be large enough to hold all device objects
uint8_t devNext;
volatile uint32_t Device::pending;

Device::Device () {
    _id = devNext;
    devMap[devNext++] = this;
}

auto Device::dispatch () -> bool {
    uint32_t pend;
    {
        //TODO BlockIRQ crit;
        pend = pending;
        pending = 0;
    }
    if (pend == 0)
        return false;
    for (int i = 0; i < devNext; ++i)
        if (pend & (1<<i))
            devMap[i]->process();
    return true;
}

namespace hall::systick {
    void (*expirer)(uint16_t,uint16_t&);
    volatile uint32_t ticks;
    volatile uint8_t counter;
    uint8_t rate;
    pthread_t tickId;

    struct Ticker : Device {
        void process () override {
            auto now = (uint16_t) millis();
            uint16_t limit = 100;
            for (int i = 0; i < devNext; ++i)
                devMap[i]->expire(now, limit);
            expirer(now, limit);
            init(limit);
        }
    };

    Ticker ticker;

    void* tickThread (void*) {
        struct timespec const ts {0, 1'000'000}; // 1 ms
        while (true) {
            nanosleep(&ts, nullptr);
            ++ticks;
            if (--counter == 0) {
                counter = rate-1;
                ticker.interrupt();
            }
        }
    }

    void init (uint8_t ms) {
        rate = counter = ms;
        if (tickId == 0)
            pthread_create(&tickId, nullptr, tickThread, nullptr);
    }

    void deinit () {
        pthread_cancel(tickId);
        pthread_join(tickId, nullptr);
        tickId = 0;
    }

    auto millis () -> uint32_t {
        return ticks;
    }

    auto micros () -> uint16_t {
        struct timespec tv;
        clock_gettime(CLOCK_MONOTONIC, &tv);
        return tv.tv_nsec / 1000; // µs resolution
    }
}

#if DOCTEST
#include <doctest.h>

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

#endif // DOCTEST
