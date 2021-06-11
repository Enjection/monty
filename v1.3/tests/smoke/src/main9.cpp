#include "hall.h"

using namespace hall;

void boss::debugf (const char*, ...) {}

Pin leds [7];

int main () {
    systick::init();
    uint8_t mem [5000];
    pool::init(mem, sizeof mem);
    Pin::define("A6:P,A5,A4,A3,A1,A0,B3", leds, 7);

    for (int i = 0; i < 6; ++i)
        Fiber::create([](void* arg) {
            auto i = (int) arg;
            while (true) {
                leds[i] = 1;
                Fiber::msWait(100);
                leds[i] = 0;
                Fiber::msWait(100*(i+5));
            }
        }, (void*) i);

    while (Fiber::runLoop()) {
        leds[6] = 0;
        idle();
        leds[6] = 1;
    }
}
