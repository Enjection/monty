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

// TODO much of the code below is specific for STM32F4

// which bit for each UART, starting from the rts/ena offsets in RCC
// TODO this needs to move to uartInfo, and varies per family
static const uint8_t rccBits [] = {
    36, 17, 18, 19, 20, 37, 30, 31, 38, 39
};

struct Uart : Object {
    constexpr Uart (int n) : dev (findDev(uartInfo, n)) {}

    Uart (int n, uint8_t tx, uint8_t rx, uint32_t rate =115200)
        : Uart (n) { if (n == dev.num) init(tx, rx, rate); }

    auto init (uint8_t tx, uint8_t rx, uint32_t rate =115200) -> Uart& {
        // TODO need a simpler way, still using JeeH pinmodes
        auto m = (int) Pinmode::alt_out;
        jeeh::Pin t = tx; t.mode(m, findAlt(altTX, tx, dev.num));
        jeeh::Pin r = rx; r.mode(m, findAlt(altRX, rx, dev.num));

        auto b = rccBits[dev.num-1];
        Periph::bitSet(RCC+ena+4*(b/32), b%32); // enable clock

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

    constexpr static auto rst = 0x20; // RCC offset: device reset
    constexpr static auto ena = 0x40; // RCC offset: clock enable
};

int main () {
    arch::init(100*1024);

    jeeh::Pin::define("B0:P,B7:P,B14:P,A5:P,A6:P,A7:P,D14:P,D15:P,F12:P", leds, 9);

    for (int i = 0; i < 18; ++i) {
        leds[i%9] = 1;
        wait_ms(100);
        leds[i%9] = 0;
    }

    printf("main\n");

    //constexpr auto uart = 2; constexpr auto txPin = "D5", rxPin = "D6";
    //constexpr auto uart = 8; constexpr auto txPin = "F9", rxPin = "F8";
    constexpr auto uart = 10; constexpr auto txPin = "E3", rxPin = "E2";

    auto& di = findDev(uartInfo, uart);
    assert(uart == di.num);
    printf("#%d irq %d base %08x tx %d rx %d\n",
            di.num, (int) di.irq, di.base,
            findAlt(altTX, txPin, uart), findAlt(altRX, rxPin, uart));

    Uart serial (uart, altpins::Pin(txPin), altpins::Pin(rxPin), 921600);

    auto base = serial.dev.base;
    printf("sr %08x\n", MMIO32(base+0x00));
    MMIO8(base+0x04) = 'J';
    printf("sr %08x\n", MMIO32(base+0x00));
    printf("sr %08x\n", MMIO32(base+0x00));
    printf("sr %08x\n", MMIO32(base+0x00));
    printf("sr %08x\n", MMIO32(base+0x00));
    printf("dr %c\n",    MMIO8(base+0x04));
    printf("sr %08x\n", MMIO32(base+0x00));

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
