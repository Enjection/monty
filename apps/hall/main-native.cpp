#include <boss.h>

using namespace boss;

Queue timers;

extern "C" void expireTimers (uint16_t now, uint16_t& limit) {
    timers.expire(now, limit);
}

int main () {
    debugf("hello %d\n", (int) sizeof (jmp_buf));
    systick::init();

    Fiber::app = []() {
        while (true)
            for (int i = 0; i < 5; ++i) {
                debugf("i %*d\n", i+1, i);
                Fiber::suspend(timers, 100*(i+1));
            }
    };

    while (true) {
        Fiber::runLoop();
        idle();
    }
}
