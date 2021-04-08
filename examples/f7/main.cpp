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
    led.define("I1:P");

    while (true) {
        msWait(100);
        auto t = millis();
        led = (t / 100) % 5 == 0;
    }
}
