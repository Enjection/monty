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

struct {
    void (*handler)(void*);
    void* driver;
} dispatchMap [100]; // TODO wrong size

static void irqHandler () {
    uint32_t psr;
    asm volatile ("mrs %0, psr" : "=r" (psr));
    auto [handler, driver] = dispatchMap[(uint8_t) (psr-0x10)];
    handler(driver);
}

static void installIrq (IrqVec irq, void (*fun)(void*), void* obj) {
    auto n = (uint8_t) irq;
    dispatchMap[n] = { fun, obj };
    (&VTableRam().wwdg)[n] = irqHandler;

    constexpr uint32_t NVIC_ENA = 0xE000E100;
    MMIO32(NVIC_ENA+4*(n/32)) = 1 << (n%32);
}

struct Uart {
    static void uartIrq (void* ptr) { ((Uart*) ptr)->uartIrqHandler(); }
    static void rxDmaIrq (void* ptr) { ((Uart*) ptr)->rxDmaIrqHandler(); }

    Uart (int n) : dev (findDev(uartInfo, n)) {}

    Uart (int n, uint8_t tx, uint8_t rx, uint32_t rate =115200)
        : Uart (n) { if (n == dev.num) init(tx, rx, rate); }

    auto init (uint8_t tx, uint8_t rx, uint32_t rate =115200) -> Uart& {
        Periph::bitSet(RCC_APB1ENR+4*(dev.ena/32), dev.ena%32); // enable clock
        Periph::bitSet(RCC_AHB1ENR, 21+dev.rxDma); // enable DMA clock

        // TODO need a simpler way, still using JeeH pinmodes
        auto m = (int) Pinmode::alt_out;
        jeeh::Pin t ('A'+(tx>>4), tx&0x1F); t.mode(m, findAlt(altTX, tx, dev.num));
        jeeh::Pin r ('A'+(rx>>4), rx&0x1F); r.mode(m, findAlt(altRX, rx, dev.num));

        dmaRX(0x14) = sizeof rxBuf;     // SxNDTR
        dmaRX(0x18) = dev.base + dr;    // SxPAR
        dmaRX(0x1C) = (uint32_t) rxBuf; // SxM0AR
        dmaRX(0x10) = // SxCR: CHSEL, MINC, CIRC, TCIE, HTIE, EN
            (dev.rxChan<<25) | (1<<10) | (1<<8) | (1<<4) | (1<<3) | (1<<0);

        baud(rate, F_CPU); // assume that the clock is running full speed

        MMIO32(dev.base+cr1) =
            (1<<13) | (1<<4) | (1<<3) | (1<<2); // UE, IDLEIE, TE, RE
        MMIO32(dev.base+cr3) = (1<<6); // DMAR

        installIrq(dev.irq, uartIrq, this);
        installIrq(IrqVec::DMA2_Stream3, rxDmaIrq, this);

        return *this;
    }

    void uartIrqHandler () {
        auto status = MMIO32(dev.base+sr);
        if (status & (1<<4)) { // IDLE
            MMIO32(dev.base+dr); // clears idle interrupt
            rxFill = sizeof rxBuf - dmaRX(0x14); // SxNDTR
        }
    }

    void rxDmaIrqHandler () {
        auto stream = dev.rxStream;
        auto status = dmaBase(stream/4); // LISR or HISR
        dmaBase(0x08+stream/4) = status; // LIFCR or HIFCR
        rxFill = status & (1<<(2+8*(stream%4))) ? sizeof rxBuf / 2 : // CHTIF
                 status & (1<<(3+8*(stream%4))) ? sizeof rxBuf : 0;  // CTCIF
    }

    void baud (uint32_t rate, uint32_t hz =defaultHz) {
        MMIO32(dev.base+brr) = (hz + rate/2) / rate;
    }

    DevInfo dev;
    volatile uint16_t rxFill = 0;
    uint8_t rxBuf [10]; // TODO volatile?
private:
    auto dmaBase (int off) -> volatile uint32_t& {
        return MMIO32((dev.rxDma == 0 ? DMA1 : DMA2) + off);
    }
    auto dmaRX (int off) -> volatile uint32_t& {
        return dmaBase(off + 0x18 * dev.rxStream);
    }
    auto dmaTX (int off) -> volatile uint32_t& {
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
    const auto uart8  = (uint8_t)  8, tx8  = Pin("F9"), rx8  = Pin("F8");
    const auto uart9  = (uint8_t)  9, tx9  = Pin("G1"), rx9  = Pin("G0");
    const auto uart10 = (uint8_t) 10, tx10 = Pin("E3"), rx10 = Pin("E2");

    pinInfo(uart2 , tx2 , rx2 );
    pinInfo(uart8 , tx8 , rx8 );
    pinInfo(uart9 , tx9 , rx9 );
    pinInfo(uart10, tx10, rx10);
wait_ms(2); // FIXME ???

    //Uart ser2  (uart2 , tx2 , rx2);
    //Uart ser8  (uart8 , tx8 , rx8);
    //Uart ser9  (uart9 , tx9 , rx9);
    Uart ser10 (uart10, tx10, rx10, 2500000);

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
    char buf [20];
    for (int i = 0; i < 6; ++i) {
        //sprintf(buf, "Hello world %d%d%d\n", i, i, i);
        //sprintf(buf, "Hi%d", i);
        sprintf(buf, "Hello%d", i);
        for (auto s = buf; *s != 0; ++s) {
            while ((MMIO32(base) & (1<<7)) == 0) {}
            MMIO8(base+0x04) = *s;
        }
        wait_ms(250);
        for (size_t i = 0; i < sizeof ser10.rxBuf; ++i)
            printf(" %02x", ser10.rxBuf[i]);
        printf(" rxFill %d\n", ser10.rxFill);
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
