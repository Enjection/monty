#include <hall.h>

using namespace hall;

int main () {
    Pin leds [7];
    Pin::define("A6:P,A5,A4,A3,A1,A0,B3", leds, 7);

    systick::init(); // defaults to 100 ms

    for (int n = 0; n < 50; ++n) {
        for (int i = 0; i < 6; ++i)
            leds[i] = n % (i+2) == 0;

        leds[6] = 0;
        asm ("wfi");
        leds[6] = 1;
    }
}
