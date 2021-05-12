#include "hall.h"
#include <pthread.h>
#include <ctime>

using namespace hall;

void hall::idle () {
    auto t = systick::millis();
    while (t == systick::millis()) {
        timespec ts { 0, 100'000 };
        nanosleep(&ts, &ts); // 100 µs, i.e. 10% of ticks' 1 ms resolution
    }
}

#if 0 // not used
auto hall::micros () -> uint64_t {
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC, &tv);
    return tv.tv_sec * 1000000LL + tv.tv_nsec / 1000; // µs resolution
}
#endif

void hall::systemReset () {
    exit(1);
}

uint8_t devNext;
volatile uint32_t Device::pending;

Device::Device () {
    _id = devNext++;
}

namespace hall::systick {
    volatile uint32_t ticks;
    volatile uint8_t counter;
    uint8_t rate;
    pthread_t tickId;

    struct Ticker : Device {
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
}
