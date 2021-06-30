#define BIOS_MAIN 1
#include <bios.h>

void appMain () {
    while (true) {
        OS.printf("<%d>\n", OS.now());
        OS.led(1);
        OS.delay(100);
        OS.led(0);
        OS.delay(900);
    }
}
