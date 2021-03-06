#include <mcu.h>

using namespace mcu;

// TODO the mcu::Pin struct clashes with the altpins::Pin function
mcu::Pin leds [7];

void mcu::failAt (void const*, void const*) {
    while (true) {
        leds[0].toggle(); // fast blink
        for (uint32_t i = 0; i < systemClock() >> 8; ++i) {}
    }
}

void mcu::idle () {
    leds[1] = 0;
    asm ("wfi");
    leds[1] = 1;
}

int main () {
    Pin::define("A6:P,A5,A4,A3,A1,A0,B3", leds, 7);

    while (true) {
        msWait(100);
        auto t = millis();
        for (int i = 2; i < 7; ++i)
            leds[i] = (t / 100) % i == 0;
    }
}
