#include "os.h"

void appMain () {
    while (true) {
        OS.printf("<%u>\n", OS.now());
        OS.led(1);
        OS.delay(10);
        OS.led(0);
        OS.delay(490);
    }
}
