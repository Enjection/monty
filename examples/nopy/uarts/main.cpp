#include <monty.h>
#include <arch.h>
#include <jee.h>
#include <jee-stm32.h>
#include <cassert>

namespace altpins {
#include "altpins.h"
}

using namespace monty;
using namespace device;
using namespace altpins;

// led 1 = B0  #0 green
// led 2 = B7  #1 blue
// led 3 = B14 #2 red
// led A = A5  #3 white
// led B = A6  #4 blue
// led C = A7  #5 green
// led D = D14 #6 yellow
// led E = D15 #7 orange
// led F = F12 #8 red

jeeh::Pin leds [9];

// TODO some of the code below is still family-specific

struct Uart : Object {
    constexpr Uart (int n) : dev (findDev(uartInfo, n)) {}

    Uart (int n, uint8_t tx, uint8_t rx, uint32_t rate =115200)
        : Uart (n) { if (n == dev.num) init(tx, rx, rate); }

    auto init (uint8_t tx, uint8_t rx, uint32_t rate =115200) -> Uart& {
        // TODO need a simpler way, still using JeeH pinmodes
        auto m = (int) Pinmode::alt_out;
        jeeh::Pin t = tx; t.mode(m, findAlt(altTX, tx, dev.num));
        jeeh::Pin r = rx; r.mode(m, findAlt(altRX, rx, dev.num));

        Periph::bitSet(RCC_ENA+4*(dev.ena/32), dev.ena%32); // enable clock

        baud(rate, F_CPU); // assume PLL has already been set up
        MMIO32(dev.base+cr1) = (1<<13) | (1<<3) | (1<<2);  // UE, TE, RE

        return *this;
    }

    void baud (uint32_t rate, uint32_t hz =defaultHz) {
        MMIO32(dev.base+brr) = (hz + rate/2) / rate;
    }

    DevInfo const& dev;
private:
    constexpr static auto sr  = 0x00; // status
    constexpr static auto dr  = 0x04; // data
    constexpr static auto brr = 0x08; // baud rate
    constexpr static auto cr1 = 0x0C; // control reg 1
};

void pinInfo (int uart, int tx, int rx) {
    auto& di = findDev(uartInfo, uart);
    assert(uart == di.num);
    printf("#%d irq %d base %08x tx %d rx %d\n",
            di.num, (int) di.irq, di.base,
            findAlt(altTX, tx, uart), findAlt(altRX, rx, uart));
}

int main () {
    arch::init(100*1024);

    jeeh::Pin::define("B0:P,B7:P,B14:P,A5:P,A6:P,A7:P,D14:P,D15:P,F12:P", leds, 9);

    for (int i = 0; i < 18; ++i) {
        leds[i%9] = true;
        wait_ms(100);
        leds[i%9] = false;
    }

    using altpins::Pin; // not the one from JeeH

    const auto uart2  =  2; const auto tx2  = Pin("D5"), rx2  = Pin("D6");
    const auto uart8  =  8; const auto tx8  = Pin("F9"), rx8  = Pin("F8");
    const auto uart9  =  9; const auto tx9  = Pin("G1"), rx9  = Pin("G0");
    const auto uart10 = 10; const auto tx10 = Pin("E3"), rx10 = Pin("E2");

    pinInfo(uart2 , tx2 , rx2 );
    pinInfo(uart8 , tx8 , rx8 );
    pinInfo(uart9 , tx9 , rx9 );
    pinInfo(uart10, tx10, rx10);

    Uart ser2  (uart2 , tx2 , rx2);
    Uart ser8  (uart8 , tx8 , rx8);
    Uart ser9  (uart9 , tx9 , rx9);
    Uart ser10 (uart10, tx10, rx10, 921600);

    auto base = ser10.dev.base;
#if 0
    printf("sr %08x\n", MMIO32(base+0x00));
    MMIO8(base+0x04) = 'J';
    printf("sr %08x\n", MMIO32(base+0x00));
    printf("sr %08x\n", MMIO32(base+0x00));
    printf("sr %08x\n", MMIO32(base+0x00));
    printf("sr %08x\n", MMIO32(base+0x00));
    printf("dr %c\n",    MMIO8(base+0x04));
    printf("sr %08x\n", MMIO32(base+0x00));
#endif
    for (auto s = "Hello world!"; *s != 0; ++s) {
        MMIO8(base+0x04) = *s;
        for (int i = 0; i < 250; ++i) {
            if (MMIO32(base+0x00) & (1<<5))
                printf("dr %c\n",    MMIO8(base+0x04));
            wait_ms(1);
        }
    }

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
