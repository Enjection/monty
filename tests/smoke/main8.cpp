#include "boss.h"
#include "hall.h"

using namespace boss;
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

void Fiber::app () {
    Pin led;
    led.config("B3:P");

    while (true) {
        led = 1;
        msWait(100);
        led = 0;
        msWait(400);
        printf("%u\n", systick::millis());
    }
}

int main () {
    systick::init();
    uart::init(2, "A2:PU7,A15:PU3", 115200);
    printf("abc\n");

    while (Fiber::runLoop())
        idle();
}
