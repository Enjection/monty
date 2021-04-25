#include <hall.h>

using hall::Pin;

int main () {
    Pin leds [7];
    Pin::define("A6:P,A5,A4,A3,A1,A0,B3", leds, 7);

    for (int n = 0; n < 100; ++n) {
        for (int i = 0; i < 7; ++i)
            leds[i] = n % (i+2) == 0;
        for (int i = 0; i < 100000; ++i)
            asm ("");
    }
}
