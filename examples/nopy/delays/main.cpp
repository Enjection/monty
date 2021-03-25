#include <monty.h>
#include <arch.h>
#include <jee.h>
#include <jee-stm32.h>
#include <cassert>

using namespace monty;
using namespace device;

// led 1 = B3
// led A = A6
// led B = A5
// led C = A4
// led D = A3
// led E = A1
// led F = A0

jeeh::Pin leds [7];

// Simple delay via polling: add the current stacklet (i.e. ourselves)
// to the end of the ready queue until requested time delay has passed.
// This also works outside stacklets (by falling back to a busy wait).

void delay_ms (uint32_t ms) {
    auto now = ticks;
    while (ticks - now < ms)
        if (Stacklet::current != nullptr)
            Stacklet::yield();
}

struct Toggler : Stacklet {
    int num;

    Toggler (int n) : num (n) {}

    auto run () -> bool override {
        delay_ms(100 * (num+1));
        leds[num] = true;
        delay_ms(100);
        leds[num] = false;
        return true;
    }
};

int main () {
    arch::init(12*1024);

    jeeh::Pin::define("B3:P,A6:P,A5:P,A4:P,A3:P,A1:P,A0:P", leds, 7);

#if 0
    for (int i = 0; i < 21; ++i) {
        leds[i%7] = true;
        wait_ms(100);
        leds[i%7] = false;
    }
#else
    // create 7 stacklets, with different delays
    for (int i = 0; i < 7; ++i)
        Stacklet::ready.append(new Toggler (i));
#endif

    printf("main\n");

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
