#include <monty.h>
#include <arch.h>

using namespace monty;

// led A = A6 #0 white
// led B = A5 #1 blue
// led C = A4 #2 green
// led D = A3 #3 yellow
// led E = A1 #4 orange
// led F = A0 #5 red
//
// led 1 = B3 #6 green (on-board) - off while idling

jeeh::Pin leds [7];

arch::Ticker* ticker;

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
        if (++count >= 10) {
            current = nullptr;
            return false;
        }
        return true;
    }
};

int main () {
    arch::init(12*1024);
    printf("main\n");

    jeeh::Pin::define("A6:P,A5:P,A4:P,A3:P,A1:P,A0:P,B3:P", leds, 7);

    // create 6 stacklets, with different delays
    for (int i = 0; i < 6; ++i)
        Stacklet::ready.append(new Toggler (i));

    auto task = arch::cliTask();
    if (task != nullptr)
        Stacklet::ready.append(task);
    else
        printf("no task\n");

    arch::Ticker t;
    ticker = &t;

    while (Stacklet::runLoop()) {
        leds[6] = 0;
        arch::idle();
        leds[6] = 1;
    }

    printf("done\n");
    arch::done();
}
