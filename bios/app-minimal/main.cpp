#define BIOS_INIT 1
#include <bios.h>
using namespace bios;

int main () {
    extern int _etext [], _sdata [], _ebss [];
    printf("code 0x%p..0x%p, data 0x%p..0x%p, " __DATE__ " " __TIME__ "\n",
            &app, _etext, _sdata, _ebss);

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
