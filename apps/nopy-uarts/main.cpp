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

// see https://interrupt.memfault.com/blog/arm-cortex-m-exceptions-and-nvic

uint8_t irqArg [100]; // TODO wrong size

struct Uart* uartMap [sizeof uartInfo / sizeof *uartInfo];

void nvicEnable (uint8_t irq, uint8_t arg, void (*fun)()) {
    assert(irq < sizeof irqArg);
    irqArg[irq] = arg;

    (&VTableRam().wwdg)[irq] = fun;

    constexpr uint32_t NVIC_ENA = 0xE000E100;
    MMIO32(NVIC_ENA+4*(irq/32)) = 1 << (irq%32);
}

template< size_t N >
void configAlt (AltPins const (&map) [N], int pin, int dev) {
    auto n = findAlt(map, pin, dev);
    if (n > 0) {
        jeeh::Pin t ('A'+(pin>>4), pin&0xF);
        t.mode((int) Pinmode::alt_out, n); // TODO still using JeeH pinmodes
    }
}

static void systemReset [[noreturn]] () {
    // ARM Cortex specific
    MMIO32(0xE000ED0C) = (0x5FA<<16) | (1<<2); // SCB AIRCR reset
    while (true) {}
}

#if STM32F4
#include "uart-f4.h"
#elif STM32L4
#include "uart-l4.h"
#endif

void pinInfo (int uart, int tx, int rx) {
    auto& di = findDev(uartInfo, uart);
    assert(uart == di.num);
    printf("#%2d ena %d irq %d base %08x tx %d rx %d\n",
            di.num, di.ena, (int) di.irq, di.base,
            findAlt(altTX, tx, uart), findAlt(altRX, rx, uart));
}

void delay (uint32_t ms) {
    auto start = ticks;
    while (ticks - start < ms)
        Stacklet::current->yield();
}

struct Listener : Stacklet {
    Listener (Uart& uart) : uart (uart) {}

    auto run () -> bool override {
        uart.wait();
        uart.clear();
        printf("\t\t%2d f %d @ %d\n", uart.dev.num, uart.rxFill(), ticks);
        return true;
    }

    Uart& uart;
};

struct Talker : Stacklet {
    Talker (Uart& uart, int ms) : uart (uart), ms (ms) {}

    auto run () -> bool override {
        delay(ms);
        static char buf [20];
        sprintf(buf, "@ %d -> %d", ticks, uart.dev.num);
        printf("%s [%d]\n", buf, strlen(buf)); delay(3);
        uart.txStart(buf, strlen(buf));
        return true;
    }

    Uart& uart;
    int ms;
};

void initLeds (char const* def, int num) {
    jeeh::Pin::define(def, leds, num);

    for (int i = 0; i < 2*num; ++i) {
        leds[i%num] = true;
        wait_ms(100);
        leds[i%num] = false;
    }
}

int main () {
    arch::init(12*1024);

#if STM32F4
    initLeds("B0:P,B7:P,B14:P,A5:P,A6:P,A7:P,D14:P,D15:P,F12:P", 9);

    using altpins::Pin; // not the one from JeeH

    const auto uart2  = (uint8_t)  2, tx2  = Pin("A2"), rx2  = Pin("A3");
    const auto uart9  = (uint8_t)  9, tx9  = Pin("G1"), rx9  = Pin("G0");
    const auto uart10 = (uint8_t) 10, tx10 = Pin("E3"), rx10 = Pin("E2");

    pinInfo(uart2 , tx2 , rx2 );
    pinInfo(uart9 , tx9 , rx9 );
    pinInfo(uart10, tx10, rx10);

    Uart ser2 (uart2);
    ser2.init();
    ser2.baud(921600, F_CPU);
    configAlt(altTX, tx2, 2);
    configAlt(altRX, rx2, 2);

    Uart ser9 (uart9);
    ser9.init();
    ser9.baud(921600, F_CPU);
    configAlt(altTX, tx9, 9);
    configAlt(altRX, rx9, 9);

    Uart ser10 (uart10);
    ser10.init();
    ser10.baud(921600, F_CPU);
    configAlt(altTX, tx10, 10);
    configAlt(altRX, rx10, 10);

    //printf("uart %d b\n", sizeof ser2);

#if 0
    auto base = ser2.dev.base;
    printf("sr %08x\n", MMIO32(base+0x00));
    MMIO8(base+0x04) = 'J';
    printf("sr %08x\n", MMIO32(base+0x00));
    printf("sr %08x\n", MMIO32(base+0x00));
    printf("sr %08x\n", MMIO32(base+0x00));
    printf("sr %08x\n", MMIO32(base+0x00));
    printf("dr %c\n",    MMIO8(base+0x04));
    printf("sr %08x\n", MMIO32(base+0x00));
#endif
#if 0
    memset(ser2.rxBuf, '.', sizeof ser2.rxBuf);
    char buf [20];
    for (int i = 0; i < 6; ++i) {
        //sprintf(buf, "Hello world %d%d!", i, i); // 15
        sprintf(buf, "Hello%d", i); // 6
        //sprintf(buf, "Hi%d?", i); // 4
        ser2.txStart(buf, 3);
        ser2.txNext = buf + 3;
        ser2.txFill = strlen(buf) - 3;
        wait_ms(100);
        for (size_t i = 0; i < sizeof ser2.rxBuf; ++i)
            printf(" %c", ser2.rxBuf[i]);
        printf("   fill %d\n", ser2.rxFill());
        wait_ms(10);
    }
#endif
#if 1
    Stacklet::ready.append(new Listener (ser2));
    Stacklet::ready.append(new Listener (ser9));
    Stacklet::ready.append(new Listener (ser10));
    Stacklet::ready.append(new Talker (ser10, 500));
    Stacklet::ready.append(new Talker (ser2, 1600));
    Stacklet::ready.append(new Talker (ser9, 2700));
#endif
#elif STM32L4
    initLeds("A6:P,A5:P,A4:P,A3:P,A1:P,A0:P,B3:P", 7);

    using altpins::Pin; // not the one from JeeH

    const auto uart1  = (uint8_t) 1, tx1  = Pin("A9"), rx1  = Pin("A10");

    pinInfo(uart1 , tx1 , rx1 );

    Uart ser1 (uart1);
    ser1.init();
    ser1.baud(921600, F_CPU);
    configAlt(altTX, tx1, 1);
    configAlt(altRX, rx1, 1);

#if 0
    auto base = ser1.dev.base;
    printf("sr %08x\n", MMIO32(base+0x1C));
    MMIO8(base+0x28) = 'L';
    printf("sr %08x\n", MMIO32(base+0x1C));
    printf("sr %08x\n", MMIO32(base+0x1C));
    printf("sr %08x\n", MMIO32(base+0x1C));
    printf("sr %08x\n", MMIO32(base+0x1C));
    printf("dr %c\n",    MMIO8(base+0x24));
    printf("sr %08x\n", MMIO32(base+0x1C));
#endif
#if 1
    Stacklet::ready.append(new Listener (ser1));
    Stacklet::ready.append(new Talker (ser1, 500));
#endif
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
