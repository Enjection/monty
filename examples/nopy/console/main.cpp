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

struct Serial : Uart {
    Serial (int num, char const* txDef, char const* rxDef) : Uart (num) {
        assert(dev.num > 0);
        init();
        baud(57600, F_CPU);
        configAlt(altTX, altpins::Pin(txDef), num);
        configAlt(altRX, altpins::Pin(rxDef), num);
    }

    struct Chunk { uint8_t* ptr; uint32_t len; };

    auto receive () -> Chunk {
        while (true) {
            auto end = rxFill();
            if (end != rxTake) {
                if (end < rxTake)
                    end = sizeof rxBuf;
                assert(end > rxTake);
                return { rxBuf+rxTake, (uint32_t) (end-rxTake) };
            }
            wait();
            clear();
        }
    }

    void advance (uint32_t n) {
        rxTake = (rxTake + n) % sizeof rxBuf;
    }

    void send (void const* ptr, uint16_t len) {
        while (txBusy()) {
            wait();
            clear();
        }
        if (len > 0)
            txStart(ptr, len);
    }

private:
    uint16_t rxTake = 0;
};

void delay (uint32_t ms) {
    auto start = ticks;
    while (ticks - start < ms)
        Stacklet::current->yield();
}

struct Listener : Stacklet {
    Listener (Serial& uart) : uart (uart) {}

    auto run () -> bool override {
        auto [ptr, len] = uart.receive();

        char buf [len+1];
        memcpy(buf, ptr, len);
        buf[len] = 0;

        printf("%s", buf);
        uart.advance(len);
        return true;
    }

    Serial& uart;
};

struct Talker : Stacklet {
    Talker (Serial& uart) : uart (uart) {}

    auto run () -> bool override {
        uart.send(nullptr, 0);
#if 1
        static char buf [20];
        static int n;
        sprintf(buf, "%d\n", ++n);
        uart.send(buf, strlen(buf));
#else
        delay(1000);
        uart.send("haha\n", 5);
        delay(1000);
        uart.send("howdy\n", 6);
        delay(1000);
        uart.send("boom!\n\3\3", 8);
#endif
        return true;
    }

    Serial& uart;
};

void initLeds (char const* def, int num) {
    jeeh::Pin::define(def, leds, num);

    for (int i = 0; i < 2*num; ++i) {
        leds[i%num] = true;
        wait_ms(50);
        leds[i%num] = false;
    }
}

int main () {
    arch::init(12*1024);
#if STM32F4
    initLeds("B0:P,B7:P,B14:P,A5:P,A6:P,A7:P,D14:P,D15:P,F12:P", 9);
    Serial serial (2, "A2", "A3");
    //Serial serial (3, "D8", "D9");
#elif STM32L4
    initLeds("A6:P,A5:P,A4:P,A3:P,A1:P,A0:P,B3:P", 7);
    Serial serial (1, "A9", "A10");
    //Serial serial (2, "A2", "A15");
#endif
    printf("main\n");

    Stacklet::ready.append(new Listener (serial));
    Stacklet::ready.append(new Talker (serial));

    auto task = arch::cliTask();
    if (task != nullptr)
        Stacklet::ready.append(task);
    else
        printf("no task\n");

    while (Stacklet::runLoop()) {
        leds[6] = 0;
        arch::idle();
        leds[6] = 1;
    }

    printf("done\n");
    arch::done();
}
