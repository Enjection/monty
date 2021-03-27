#include <monty.h>
#include <arch.h>
#include <jee.h>
#include <jee-stm32.h>
#include <cassert>

using namespace monty;
using namespace device;

// led A = A6 #0 white
// led B = A5 #1 blue
// led C = A4 #2 green
// led D = A3 #3 yellow
// led E = A1 #4 orange
//
// led F = A0 #5 red - on while in expiration loop
// led 1 = B3 #6 green (on-board) - off while idling

jeeh::Pin leds [7];

struct Ticker : Event {
    Ticker () {
        tickId = regHandler();

        VTableRam().systick = []() {
            ++ticks;
            Stacklet::setPending(tickId);
        };
    }
    ~Ticker () {
printf("~Ticker?\n");
        VTableRam().systick = []() { ++ticks; }; // perhaps just disable it
        deregHandler();
    }

    auto next () -> Value override {
        leds[5] = 1;
        auto t = triggerExpired(ticks);
        leds[5] = 0;
        //assert(t > 0);
        return {};
    }

    static uint8_t tickId;
};

uint8_t Ticker::tickId;

Ticker* ticker;

void msWait (uint32_t ms) { ticker->wait(ms); }

struct Toggler : Stacklet {
    int num;
    int count = 0;

    Toggler (int n) : num (n) {}

    auto run () -> bool override {
        msWait(100 * (num+1));
        leds[num] = 1;
        msWait(100);
        leds[num] = 0;
        return true; //++count < 10;
    }
};

int main () {
    arch::init(12*1024);

    jeeh::Pin::define("A6:P,A5:P,A4:P,A3:P,A1:P,A0:P,B3:P", leds, 7);

#if 0
    for (int i = 0; i < 21; ++i) {
        leds[i%7] = true;
        wait_ms(100);
        leds[i%7] = false;
    }
#else
    // create 5 stacklets, with different delays
    for (int i = 0; i < 5; ++i)
        Stacklet::ready.append(new Toggler (i));
#endif

    printf("main\n");

    auto task = arch::cliTask();
    if (task != nullptr)
        Stacklet::ready.append(task);
    else
        printf("no task\n");

    ticker = new Ticker;

    while (Stacklet::runLoop()) {
        leds[6] = 0;
        arch::idle();
        leds[6] = 1;
    }

    delete ticker;

    printf("done\n");
    arch::done();
}
