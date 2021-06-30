#define BIOS_MAIN 1
#include <bios.h>

using namespace bios;

void appMain () {
    while (true) {
        printf("<%d>\n", now());
        led(1);
        delay(100);
        led(0);
        delay(900);
    }
}
