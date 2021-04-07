#include <monty.h>
#include <mcu.h>

using namespace monty;
using namespace mcu;

// TODO the mcu::Pin struct clashes with the altpins::Pin function
mcu::Pin leds [7];

void mcu::failAt (void const* pc, void const* lr) {
    while (true) {
        leds[0].toggle(); // fast blink
        for (uint32_t i = 0; i < systemClock() >> 8; ++i) {}
    }
}

#if 0
void mcu::idle () {
    leds[1] = 1;
    asm ("cpsie i");
    if (Stacklet::current != nullptr)
        Stacklet::current->yield();
    leds[1] = 0;
}
#endif

// Simple delay via polling: add the current stacklet (i.e. ourselves)
// to the end of the ready queue until requested time delay has passed.
// This also works outside stacklets (by falling back to a busy wait).

auto monty::nowAsTicks () -> uint32_t {
    return millis();
}

void delay (uint32_t n) {
#if 0
    while (n-- > 0)
        msWait(1);
#else
    auto now = nowAsTicks();
    while (nowAsTicks() - now < n)
        if (Stacklet::current != nullptr)
            Stacklet::current->yield();
#endif
}

struct Toggler : Stacklet {
    int num;

    Toggler (int n) : num (n) {}

    auto run () -> bool override {
        leds[num] = 1;
        delay(1);
        leds[num] = 0;
        delay(num+1);
        return true;
    }
};

int main () {
    mcu::Pin::define("A6:P,A5:P,A4:P,A3:P,A1:P,A0:P,B3:P", leds, 7);
    msWait(100); // start systick at 10 Hz

    char mem [2000];
    gcSetup(mem, sizeof mem); // set up a small GC memory pool

    // create 5 stacklets, with different delays
    for (int i = 2; i < 7; ++i)
        Stacklet::ready.append(new Toggler (i));

    // minimal loop to keep stacklets going
    while (Stacklet::runLoop()) {
        leds[0] = 1;
        BlockIRQ crit;
        if (Stacklet::pending == 0)
            asm ("wfi");
        leds[0] = 0;
    }
}
