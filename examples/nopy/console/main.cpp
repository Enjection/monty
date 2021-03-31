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

uint8_t irqMap [100];       // TODO wrong size
struct Device* devMap [20]; // large enough to handle all device objects
static_assert(sizeof devMap / sizeof *devMap < 256, "doesn't fit in irqMap");

struct Device : Event {
    Device () { regHandler(); }
    ~Device () override { deregHandler(); } // TODO clear device slot

    virtual void irqHandler () =0;

    // install the uart IRQ dispatch handler in the hardware IRQ vector
    void installIrq (uint32_t irq) {
        assert(irq < sizeof irqMap);
        for (auto& e : devMap) {
            if (e == nullptr)
                e = this;
            if (e == this) {
                irqMap[irq] = &e - devMap;

                // TODO could be hard-coded, no need for RAM-based irq vector
                (&VTableRam().wwdg)[irq] = []() {
                    auto vecNum = MMIO8(0xE000ED04); // ICSR
                    auto idx = irqMap[vecNum-0x10];
                    devMap[idx]->irqHandler();
                };

                constexpr uint32_t NVIC_ENA = 0xE000E100;
                MMIO32(NVIC_ENA+4*(irq/32)) = 1 << (irq%32);

                return;
            }
        }
        assert(false); // ran out of unused device slots
    }

    void trigger () { Stacklet::setPending(_id); }

    template< size_t N >
    static void configAlt (AltPins const (&map) [N], int pin, int dev) {
        auto n = findAlt(map, pin, dev);
        if (n > 0) {
            jeeh::Pin t ('A'+(pin>>4), pin&0xF);
            auto m = (int) Pinmode::alt_out | (int) Pinmode::in_pullup;
            t.mode(m, n); // TODO still using JeeH pinmodes
        }
    }

    static void reset [[noreturn]] () {
        // ARM Cortex specific
        MMIO32(0xE000ED0C) = (0x5FA<<16) | (1<<2); // SCB AIRCR reset
        while (true) {}
    }
};

#if STM32F4
#include "uart-f4.h"
#elif STM32L4
#include "uart-l4.h"
#endif

struct Serial : Uart {
    Serial (int num, char const* txDef, char const* rxDef) : Uart (num) {
        assert(dev.num > 0);
        configAlt(altTX, altpins::Pin(txDef), num);
        configAlt(altRX, altpins::Pin(rxDef), num);
        init();
        baud(230400, F_CPU);
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
            printf("(");
            wait();
            clear();
            printf(")");
        }
    }

    void advance (uint32_t n) {
        rxTake = (rxTake + n) % sizeof rxBuf;
    }

    void send (void const* ptr, uint16_t len) {
        while (txBusy()) {
            printf("[");
            wait();
            clear();
            printf("]");
        }
        printf("%d>", len);
        txStart(ptr, len);
    }

private:
    uint16_t rxTake = 0;
};

static Serial* outDev;

static char outBuf [50];
static uint16_t outFill;

static void outFlush () {
    if (outFill > 0) {
        assert(outDev != nullptr);
        outDev->send(outBuf, outFill);
        outDev->send(nullptr, 0);
        outFill = 0;
    }
}

static void outFun (int ch) {
    outBuf[outFill++] = ch;
    if (outFill >= sizeof outBuf)
        outFlush();
}

static int debugf(char const* fmt, ...) {
    va_list ap; va_start(ap, fmt); veprintf(outFun, fmt, ap); va_end(ap);
    outFlush();
    return 0;
}

void delay (uint32_t ms) {
    auto start = ticks;
    while (ticks - start < ms)
        Stacklet::current->yield();
}

struct Listener : Stacklet {
    Listener (Serial& uart) : uart (uart) {}

    auto run () -> bool override {
        printf("l");
        uart.send(nullptr, 0);
//static bool first = true; if (first) { uart.send("hey!",4); first = false; }

        auto [ptr, len] = uart.receive();
        assert(len > 0);

        static char buf [1000];
        if (len > sizeof buf)
            len = sizeof buf;
        memcpy(buf, ptr, len);
        uart.advance(len);

        printf("%d<", len);

        printf("R");
        uart.send(buf, len);

        printf("L");
        return true;
    }

    Serial& uart;
};

struct Talker : Stacklet {
    Talker (Serial& uart) : uart (uart) {}

    auto run () -> bool override {
        debugf("bingo %d\n", ticks);
        printf("t");
        uart.send(nullptr, 0);
#if 0
        static char buf [20];
        static int n;
        sprintf(buf, "%d\n", ++n);
        uart.send(buf, strlen(buf));
#else
        delay(500);
        debugf("<haha>");
        delay(500);
        debugf("<howdy>");
        delay(5000);
        debugf("<ha>");
#endif
        printf("T\n");
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

#include <unistd.h> // for sbrk
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
    outDev = &serial;

    printf("main\n");

    Stacklet::ready.append(new Listener (serial));
    Stacklet::ready.append(new Talker (serial));

#if 0
    auto task = arch::cliTask();
    if (task != nullptr)
        Stacklet::ready.append(task);
    else
        printf("no task\n");
#endif

    while (Stacklet::runLoop()) {
        //leds[6] = 0;
        arch::idle();
        //leds[6] = 1;
    }

    printf("done\n");
    arch::done();
}
