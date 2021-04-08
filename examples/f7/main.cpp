#include <mcu.h>

#include <cstdarg>
#include <printer.h>

using namespace mcu;

mcu::Pin led;

static void swoPutc (int c) {
    constexpr auto ITM8 =  io8<0xE000'0000>;
    constexpr auto ITM  = io32<0xE000'0000>;
    enum { TER=0xE00, TCR=0xE80, LAR=0xFB0 };

    if (ITM(TCR)[0] && ITM(TER)[0]) {
        while (ITM(0)[0] == 0) {}
        ITM8(0) = c;
    }
}

Printer debugf (nullptr, [](void*, uint8_t const* ptr, int len) {
    while (--len >= 0)
        swoPutc(*ptr++);
});

void mcu::failAt (void const* pc, void const* lr) {
    debugf("failAt %p %p\n", pc, lr);
    for (uint32_t i = 0; i < systemClock() >> 15; ++i) {}
    systemReset();
}

int main () {
    fastClock();
    led.define("K3:P"); // set PK3 pin low to turn off the LCD backlight

    led.define("I1:P");

    msWait(1);
    while (true) {
        msWait(100);
        auto t = millis();
        debugf("hello %d\n", t);
        led = (t / 100) % 5 == 0;
    }
}
