#define BIOS_INIT 1
#include <bios.h>
using bios::printf;

int main () {
    extern int _etext [], _sdata [], _ebss [];
    printf("code 0x%p..0x%p, data 0x%p..0x%p, " __DATE__ " " __TIME__ "\n",
            &app, _etext, _sdata, _ebss);

    while (true) {
        bios::led(1);
        printf("<%d>\n", bios::now());
        bios::delay(100);
        bios::led(0);
        bios::delay(900);
    }
}
