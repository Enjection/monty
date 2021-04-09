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

Serial serial (1);

Printer printer (&serial, [](void* obj, uint8_t const* ptr, int len) {
    ((Serial*) obj)->send(ptr, len);
});

extern "C" int printf (const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int result = printer.vprintf(fmt, ap);
    va_end(ap);

    return result;
}

void mcu::failAt (void const* pc, void const* lr) {
    debugf("failAt %p %p\n", pc, lr);
    for (uint32_t i = 0; i < systemClock() >> 15; ++i) {}
    systemReset();
}

int main () {
    fastClock();
debugf("\nABC\n");
    led.define("K3:P"); // set PK3 pin low to turn off the LCD backlight

    led.define("I1:P");

    mcu::Pin tx, rx; // TODO use the altpins info
    tx.define("A9:PU7");
    rx.define("B7:PU7");

debugf("11\n");
    serial.init();
debugf("22\n");

    auto hz = mcu::systemClock();
    serial.baud(921600, hz/2);
debugf("33 %d Hz\n", hz);
    printf("%d Hz\n", hz);
debugf("44\n");

    msWait(1);
    while (true) {
        msWait(100);
        auto t = millis();
debugf("hello %d\n", t);
        //printf("hello %d\n", t);
        led = (t / 100) % 5 == 0;
    }
}
