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

void installIrq (uint8_t irq, void (*handler)(), uint8_t arg) {
    //printf("install %d arg %d\n", irq, arg);
    assert(irq < sizeof irqArg);
    irqArg[irq] = arg;
    (&VTableRam().wwdg)[irq] = handler;

    constexpr uint32_t NVIC_ENA = 0xE000E100;
    MMIO32(NVIC_ENA+4*(irq/32)) = 1 << (irq%32);
}

struct Uart : Event {
    Uart (int n) : dev (findDev(uartInfo, n)) {
        if (dev.num == n)
            regHandler();
        else
            dev.num = 0;
    }

    ~Uart () override { deinit(); }

    void init (uint8_t tx, uint8_t rx, uint32_t rate =115200) {
        assert(dev.num > 0);
        uartMap[dev.pos] = this;

        Periph::bitSet(RCC_APB1ENR+4*(dev.ena/32), dev.ena%32); // uart on
        Periph::bitSet(RCC_AHB1ENR, 21+dev.rxDma);              // dma on

        // TODO need a simpler way, still using JeeH pinmodes
        auto m = (int) Pinmode::alt_out;
        jeeh::Pin t ('A'+(tx>>4), tx&0x1F); t.mode(m, findAlt(altTX, tx, dev.num));
        jeeh::Pin r ('A'+(rx>>4), rx&0x1F); r.mode(m, findAlt(altRX, rx, dev.num));

        dmaRX(0x14) = sizeof rxBuf;     // SxNDTR
        dmaRX(0x18) = dev.base + dr;    // SxPAR
        dmaRX(0x1C) = (uint32_t) rxBuf; // SxM0AR
        dmaRX(0x10) = // SxCR: CHSEL, MINC, CIRC, TCIE, HTIE, EN
            (dev.rxChan<<25) | (1<<10) | (1<<8) | (1<<4) | (1<<3) | (1<<0);

        dmaTX(0x18) = dev.base + dr;    // SxPAR
        dmaTX(0x10) = // SxCR: CHSEL, MINC, DIR, TCIE
            (dev.txChan<<25) | (1<<10) | (1<<6) | (1<<4);

        baud(rate, F_CPU); // assume that the clock is running full speed

        MMIO32(dev.base+cr3) = (1<<7) | (1<<6) | (1<<0); // DMAT, DMAR, EIE
        MMIO32(dev.base+cr1) =
            (1<<13) | (1<<4) | (1<<3) | (1<<2); // UE, IDLEIE, TE, RE

        installIrq((uint8_t) dev.irq, uartIrq, dev.pos);
        installIrq(dmaInfo[dev.rxDma].streams[dev.rxStream], uartIrq, dev.pos);
        installIrq(dmaInfo[dev.txDma].streams[dev.txStream], uartIrq, dev.pos);
    }

    void deinit () {
        if (dev.num == 0)
            return;
        Periph::bitClear(RCC_APB1ENR+4*(dev.ena/32), dev.ena%32); // uart off
        Periph::bitClear(RCC_AHB1ENR, 21+dev.rxDma);              // dma off
        uartMap[dev.pos] = nullptr; // TODO can there be a pending irq?
    }

    // this is installed as IRQ handler in the hardware IRQ vector
    static void uartIrq () {
        auto vecNum = MMIO8(0xE000ED04); // ICSR
        auto arg = irqArg[vecNum-0x10];
        uartMap[arg]->uartIrqHandler();
    }

    // the actual interrupt handler, with access to the object fields
    void uartIrqHandler () {
        auto status = MMIO32(dev.base+sr);
        MMIO32(dev.base+dr); // clear idle interrupt and errors

        auto rx = dev.rxStream;
        uint8_t rxStat = dmaBase(rx&~3) >> 8*(rx&3); // LISR or HISR
        dmaBase(0x08+(rx&~3)) = rxStat << 8*(rx&3);  // LIFCR or HIFCR

        // whatever the interrupt cause, update the fill index
        //rxFill = sizeof rxBuf - dmaRX(0x14); // SxNDTR

        auto tx = dev.txStream;
        uint8_t txStat = dmaBase(tx&~3) >> 8*(tx&3); // LISR or HISR
        dmaBase(0x08+(tx&~3)) = txStat << 8*(tx&3);  // LIFCR or HIFCR

        if ((txStat & (1<<3)) != 0 && txFill > 0)    // TCIF
            txStart(txNext, txFill);

        Stacklet::setPending(_id);
    }

    void baud (uint32_t rate, uint32_t hz =defaultHz) const {
        MMIO32(dev.base+brr) = (hz + rate/2) / rate;
    }

    auto rxFill () const -> uint16_t {
        return sizeof rxBuf - dmaRX(0x14); // SxNDTR
    }

    void txStart (void const* ptr, uint16_t len) {
        dmaTX(0x14) = len;            // SxNDTR
        dmaTX(0x1C) = (uint32_t) ptr; // SxM0AR
        dmaTX(0x10) |= (1<<0);        // EN
        txFill = 0;
    }

    DevInfo dev;
    void const* txNext;
    volatile uint16_t txFill = 0;
    uint8_t rxBuf [10]; // TODO volatile?
private:
    auto dmaBase (int off) const -> volatile uint32_t& {
        return MMIO32(dmaInfo[dev.rxDma].base + off);
    }
    auto dmaRX (int off) const -> volatile uint32_t& {
        return dmaBase(off + 0x18 * dev.rxStream);
    }
    auto dmaTX (int off) const -> volatile uint32_t& {
        return dmaBase(off + 0x18 * dev.txStream);
    }

    constexpr static auto sr  = 0x00; // status
    constexpr static auto dr  = 0x04; // data
    constexpr static auto brr = 0x08; // baud rate
    constexpr static auto cr1 = 0x0C; // control reg 1
    constexpr static auto cr3 = 0x14; // control reg 3
};

void pinInfo (int uart, int tx, int rx) {
    auto& di = findDev(uartInfo, uart);
    assert(uart == di.num);
    printf("#%2d ena %d irq %d base %08x tx %d rx %d\n",
            di.num, di.ena, (int) di.irq, di.base,
            findAlt(altTX, tx, uart), findAlt(altRX, rx, uart));
}

void delay (int ms) {
    auto start = ticks;
    while (ticks - start < ms)
        Stacklet::current->yield();
}

struct Listener : Stacklet {
    Listener (Uart& dev) : uart (dev) {}

    auto run () -> bool override {
        uart.wait();
        uart.clear();
        printf("\t\t%2d f %d @ %d\n", uart.dev.num, uart.rxFill(), ticks);
        return true;
    }

    Uart& uart;
};

struct Talker : Stacklet {
    Talker (Uart& dev, int ms) : uart (dev), period (ms) {}

    auto run () -> bool override {
        delay(period);
        static char buf [20];
        sprintf(buf, "@ %d -> %d", ticks, uart.dev.num);
        printf("%s [%d]\n", buf, strlen(buf)); delay(3);
        uart.txStart(buf, strlen(buf));
        return true;
    }

    Uart& uart;
    int period;
};

int main () {
    arch::init(100*1024);

    jeeh::Pin::define("B0:P,B7:P,B14:P,A5:P,A6:P,A7:P,D14:P,D15:P,F12:P", leds, 9);

    for (int i = 0; i < 18; ++i) {
        leds[i%9] = true;
        wait_ms(100);
        leds[i%9] = false;
    }

    using altpins::Pin; // not the one from JeeH

    const auto uart2  = (uint8_t)  2, tx2  = Pin("D5"), rx2  = Pin("D6");
    //const auto uart8  = (uint8_t)  8, tx8  = Pin("F9"), rx8  = Pin("F8");
    const auto uart9  = (uint8_t)  9, tx9  = Pin("G1"), rx9  = Pin("G0");
    const auto uart10 = (uint8_t) 10, tx10 = Pin("E3"), rx10 = Pin("E2");

    pinInfo(uart2 , tx2 , rx2 );
    //pinInfo(uart8 , tx8 , rx8 );
    pinInfo(uart9 , tx9 , rx9 );
    pinInfo(uart10, tx10, rx10);

    Uart ser2 (uart2); ser2.init(tx2 , rx2, 921600);
    //Uart ser8 (uart8); ser8.init(tx8 , rx8);
    Uart ser9 (uart9); ser9.init(tx9 , rx9, 921600);
    Uart ser10 (uart10); ser10.init(tx10, rx10, 921600);

    //printf("uart %d b\n", sizeof ser2);

#if 1
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
    printf("main\n");

#if 1
    Stacklet::ready.append(new Listener (ser2));
    Stacklet::ready.append(new Listener (ser9));
    Stacklet::ready.append(new Listener (ser10));
    Stacklet::ready.append(new Talker (ser10, 500));
    Stacklet::ready.append(new Talker (ser2, 1600));
    Stacklet::ready.append(new Talker (ser9, 2700));
#endif

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
