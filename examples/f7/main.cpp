#include <mcu.h>

using namespace mcu;

mcu::Pin led;

void mcu::failAt (void const*, void const*) {
    while (true) {
        led.toggle(); // fast blink
        for (uint32_t i = 0; i < systemClock() >> 8; ++i) {}
    }
}

int main () {
    fastClock();
    led.define("K3:P"); // set K3 to output "0", turns off the LCD backlight

    led.define("I1:P");

    while (true) {
        msWait(100);
        auto t = millis();
        led = (t / 100) % 5 == 0;
    }
}
