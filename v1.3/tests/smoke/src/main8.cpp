#include "hall.h"

using namespace hall;

extern "C" int printf (const char* fmt, ...) {
    auto uartPutc = [](void*, int c) { uart::putc(2, c); };

    va_list ap;
    va_start(ap, fmt);
    auto r = veprintf(uartPutc, nullptr, fmt, ap);
    va_end(ap);
    return r;
}

void boss::debugf (const char*, ...) __attribute__((alias ("printf")));

int main () {
    fastClock();
    //systick::init();
    uint8_t mem [5000];
    pool::init(mem, sizeof mem);
    uart::init(2, "A2:PU7,A15:PU3", 115200);

    Pin led;
    led.config("B3:P");

    Fiber::create([](void*) {
        printf("%d Hz\n", systemHz());
        for (int i = 0; true; ++i)
            printf("> %*u /\n", 76 - (i % 70), systick::millis());
    });

    while (Fiber::runLoop()) {
        led = 0;
        idle();
        led = 1;
    }
}
