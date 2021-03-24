#include <monty.h>
#include <arch.h>
#include <jee.h>
#include <jee-stm32.h>

namespace altpins {
#include "altpins.h"
}

using namespace monty;
using namespace device;
using namespace altpins;

jeeh::Pin leds [9];

// led1 B0  = 0: green
// led2 B7  = 1: blue
// led3 B14 = 2: red
// ledA A5  = 3: white
// ledB A6  = 4: blue
// ledC A7  = 5: green
// ledD D14 = 6: yellow
// ledE D15 = 7: orange
// ledF F12 = 8: red

int main () {
    arch::init(100*1024);
    printf("main\n");

    jeeh::Pin::define("B0:P,B7:P,B14:P,A5:P,A6:P,A7:P,D14:P,D15:P,F12:P", leds, 9);

    for (int i = 0; i < 27; ++i) {
        leds[i%9] = 1;
        wait_ms(100);
        leds[i%9] = 0;
    }

    auto task = arch::cliTask();
    if (task != nullptr)
        Stacklet::ready.append(task);
    else
        printf("no task\n");

    while (Stacklet::runLoop())
        arch::idle();

    printf("done\n");
    arch::done();
}
