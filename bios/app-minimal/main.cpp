#define BIOS_INIT 1
#include <bios.h>
using namespace bios;

#include <jee.h>

int main () {
    extern int _etext [], _sdata [], _ebss [];
    printf("code 0x%p..0x%p, data 0x%p..0x%p, " __DATE__ " " __TIME__ "\n",
            &app, _etext, _sdata, _ebss);

    Timer<5,2> backlight;
    backlight.init(1000, 1080); // 0..999 @ 108 MHz / 1080, i.e. 100 Hz
    backlight.pwm(100);

    PinH<11>::mode(Pinmode::alt_out, 2);

    while (true) {
        led(1);
        printf("<%d>\n", now());
        while (true)
            if (auto ch = getch(); ch >= 0)
                printf("%c", ch);
            else
                break;
        delay(100);
        led(0);
        delay(900);
    }
}
