#include <monty.h>
#include <arch.h>
#include <cassert>

using namespace monty;

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

struct Serial : arch::Uart {
    Serial (int num, char const* txDef, char const* rxDef) : Uart (num) {
        assert(dev.num > 0);
#if STM32F1
        // TODO TX::mode(Pinmode::alt_out);
        // TODO RX::mode(Pinmode::in_pullup);
        //  hard-coded cases for now ...
        switch (num) {
            case 1: PinA<9>::mode(Pinmode::alt_out);
                    PinA<10>::mode(Pinmode::in_pullup);
                    break;
            case 2: PinA<2>::mode(Pinmode::alt_out);
                    PinA<3>::mode(Pinmode::in_pullup);
                    break;
        }
#else
        configAlt(altpins::altTX, altpins::Pin(txDef), num);
        configAlt(altpins::altRX, altpins::Pin(rxDef), num);
#endif
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

        printf("%d<", (int) len);

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
#if STM32F1
    initLeds("A1:P", 1);
    Serial serial (2, "A2", "A3");
    //Serial serial (1, "A9", "A10");
#elif STM32F4
    initLeds("B0:P,B7:P,B14:P,A5:P,A6:P,A7:P,D14:P,D15:P,F12:P", 9);
    Serial serial (2, "A2", "A3");
    //Serial serial (3, "D8", "D9");
#elif STM32F7
    initLeds("A5:P,A7:P,B1:P", 3);
    Serial serial (2, "A2", "A3");
    //Serial serial (6, "C6", "C7");
#elif STM32L0
    initLeds("A5:P", 1);
    Serial serial (1, "A9", "A10");
    //Serial serial (2, "A2", "A3");
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
