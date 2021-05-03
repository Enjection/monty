#include "boss.h"
#include <pthread.h>
#include <ctime>

namespace hall {
    void idle () {
        timespec ts { 0, 100'000 };
        nanosleep(&ts, &ts); // 100 µs, i.e. 10% of ticks' 1 ms resolution
    }

#if 0 // not used
    auto micros () -> uint64_t {
        struct timespec tv;
        clock_gettime(CLOCK_MONOTONIC, &tv);
        return tv.tv_sec * 1000000LL + tv.tv_nsec / 1000; // µs resolution
    }
#endif

    [[noreturn]] void systemReset () {
        exit(1);
    }

    void debugPutc (void*, int c) {
        fputc(c, stderr);
    }

    volatile uint32_t pending;

    extern "C" void expireTimers (uint16_t, uint16_t&); // TODO yuck

    void processAllPending () {
        if (pending) { // TODO not atomic, should be harmless with just ticks
            pending = 0;

            auto now = (uint16_t) systick::millis();
            uint16_t limit = 60'000;
            expireTimers(now, limit);
            systick::init(limit < 100 ? limit : 100);
        }
    }

    namespace systick {
        volatile uint32_t ticks;
        volatile uint8_t counter;
        uint8_t rate;
        pthread_t tickId;

        void* tickThread (void*) {
            struct timespec const ts {0, 1'000'000}; // 1 ms
            while (true) {
                nanosleep(&ts, nullptr);
                ++ticks;
                if (--counter == 0) {
                    counter = rate-1;
                    ++pending;
                }
            }
        }

        void init (uint8_t ms) {
            rate = counter = ms;
            if (tickId == nullptr)
                pthread_create(&tickId, nullptr, tickThread, nullptr);
        }

        auto millis () -> uint32_t {
            return ticks;
        }
    }
}
