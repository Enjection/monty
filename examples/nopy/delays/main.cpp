#include <monty.h>
#include <arch.h>
#include <jee.h>
#include <jee-stm32.h>
#include <cassert>

using namespace monty;
using namespace device;

// led 1 = B3 #0 green
// led A = A6 #1 white
// led B = A5 #2 blue
// led C = A4 #3 green
// led D = A3 #4 yellow
// led E = A1 #5 orange
// led F = A0 #6 red

jeeh::Pin leds [7];

struct Ticker : Stacklet {
    Event ticking;

    Ticker () {
        tickId = ticking.regHandler();

        VTableRam().systick = []() {
            ++ticks;
            Stacklet::setPending(tickId);
        };
    }
    ~Ticker () {
printf("~Ticker?\n");
        VTableRam().systick = []() { ++ticks; }; // better perhaps: disable it?
        ticking.deregHandler();
    }

    auto run () -> bool override {
        ticking.wait();
        ticking.clear();
        auto t = Event::always.triggerExpired(ticks);
        assert(t > 0);
        //current = nullptr;
        yield();
        return true;
    }

    static uint8_t tickId;
};

uint8_t Ticker::tickId;

struct Toggler : Stacklet {
    int num;

    Toggler (int n) : num (n) {}

    auto run () -> bool override {
        msWait(100 * (num+1));
        leds[num] = 1;
        msWait(100);
        leds[num] = 0;
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

    Ticker ticker;
    Stacklet::ready.append(&ticker);

    while (Stacklet::runLoop())
        arch::idle();

    printf("done\n");
    arch::done();
}
