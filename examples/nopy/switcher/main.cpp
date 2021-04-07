#include <monty.h>
#include <arch.h>
#include <jee.h>

using namespace monty;

int counter, total;

struct Switcher : Stacklet {
    auto run () -> bool override {
        int buf [1]; // switch between 1 and 251 for a 1000b stack size change
        jeeh::DWT::count();
        yield();
        int n = jeeh::DWT::count();
        total += n;
        if (++counter == 1000) {
            printf("%d %d\n", total, buf[0]);
            arch::done(); // will generate a reset
        }
        return true;
    }
};

int main () {
    arch::init(3*1024);
    wait_ms(500);

    Stacklet::ready.append(new Switcher);

    while (Stacklet::runLoop()) {}

    return 0;
}
