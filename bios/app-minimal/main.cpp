#define BIOS_INIT 1
#include <bios.h>

using namespace bios;

int main () {
    printf("--- app: " __DATE__ " " __TIME__ "\n");
    while (true) {
        led(1);
        printf("<%d>\n", now());
        delay(100);
        led(0);
        delay(900);
    }
}
