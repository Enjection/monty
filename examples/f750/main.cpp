#include <mcu.h>

#include <cstdarg>
#include <printer.h>

using namespace mcu;

static Printer swoOut (nullptr, [](void*, uint8_t const* ptr, int len) {
    constexpr auto ITM8 =  io8<0xE000'0000>;
    constexpr auto ITM  = io32<0xE000'0000>;
    enum { TER=0xE00, TCR=0xE80, LAR=0xFB0 };

    if (ITM(TCR)[0] && ITM(TER)[0])
        while (--len >= 0) {
            while (ITM(0)[0] == 0) {}
            ITM8(0) = *ptr++;
        }
});

void mcu::debugf (const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    swoOut.vprintf(fmt, ap);
    va_end(ap);
}

void mcu::failAt (void const* pc, void const* lr) {
    debugf("failAt %p %p\n", pc, lr);
    for (uint32_t i = 0; i < systemClock() >> 15; ++i) {}
    systemReset();
}

Serial serial (1);

Printer printer (nullptr, [](void*, uint8_t const* ptr, int len) {
    serial.send(ptr, len);
});

extern "C" int printf (const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int result = printer.vprintf(fmt, ap);
    va_end(ap);
    return result;
}

mcu::Pin led;

int main () {
    fastClock();
debugf("\n------------\n%d Hz\n", systemClock());
    led.define("K3:P"); // set PK3 pin low to turn off the LCD backlight

    led.define("I1:P");

    mcu::Pin tx, rx; // TODO use the altpins info
    tx.define("A9:PU7");
    rx.define("B7:PU7");

    serial.init();
    serial.baud(921600, mcu::systemClock() / 2);

    while (true) {
        msWait(100);
        auto t = millis();
        led = (t / 100) % 5 == 0;
        if (led)
            printf("hello %d\n", t);
    }
}
