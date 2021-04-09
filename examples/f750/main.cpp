#include <mcu.h>

#include <cstdarg>
#include <printer.h>

using namespace mcu;

mcu::Pin led;

int main () {
    fastClock(); // OpenOCD expects a 200 MHz clock for SWO
    printf("@ %d MHz\n", systemClock() / 1'000'000);

    led.define("K3:P"); // set PK3 pin low to turn off the LCD backlight
    led.define("I1:P");

    Serial serial (1, "A9:PU7,B7:PU7"); // TODO use the altpins info
    serial.baud(921600, systemClock() / 2);

    Printer printer (&serial, [](void* obj, uint8_t const* ptr, int len) {
        ((Serial*) obj)->send(ptr, len);
    });

    stdOut = &printer;

    while (true) {
        msWait(100);
        auto t = millis();
        led = (t / 100) % 5 == 0;
        if (led)
            printf("hello %d\n", t);
    }
}
