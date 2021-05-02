#include <boss.h>

using namespace boss;

struct DmaInfo { uint32_t base; uint8_t streams [8]; };

DmaInfo const dmaInfo [] = {
    { 0x4002'0000, { 0, 11, 12, 13, 14, 15, 16, 17 }},
    { 0x4002'0400, { 0, 56, 57, 58, 59, 60, 68, 69 }},
};

struct UartInfo {
    uint8_t num :4, ena, irq;
    uint16_t dma :1, rxChan :4, rxStream :3, txChan :4, txStream :3;
    uint32_t base;

    auto dmaBase () const { return dmaInfo[dma].base; }
};

namespace hall {
#include <uart-stm32l4.h>
}

Uart uart [] = {
    UartInfo { 1, 78, 37, 0, 2, 5, 2, 4, 0x4001'3800 },
    UartInfo { 2, 17, 38, 0, 2, 6, 2, 7, 0x4000'4400 },
    UartInfo { 3, 18, 38, 0, 2, 3, 2, 2, 0x4000'4800 },
    UartInfo { 4, 19, 52, 1, 2, 5, 2, 3, 0x4000'4C00 },
};

static void uartPutc (void* o, int c) {
    static uint8_t* buf;
    static uint8_t fill;
    auto flush = [&]() {
        auto i = pool.idOf(buf);
        pool.tag(i) = fill - 1;
        ((Uart*) o)->txStart(i);
        fill = 0;
    };

    if (fill >= pool.SZBUF)
        flush();
    if (fill == 0)
        buf = pool.allocate();
    buf[fill++] = c;
    if (c == '\n')
        flush();
}

int printf (const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = veprintf(uartPutc, &uart[1], fmt, ap);
    va_end(ap);
    return n;
}

Pin leds [6];
Queue timers;

extern "C" void expireTimers (uint16_t now, uint16_t& limit) {
    timers.expire(now, limit);
}

int main () {
    fastClock();

    Pin::define("A6:P,A5,A4,A3,A1,A0", leds, sizeof leds);

    systick::init(); // defaults to 100 ms
    cycles::init();

    uart[1].init("A2:PU7,A15:PU3", 921600);
    //printf("\n");
    //asm ("wfi");
    
    debugf("hello %d\n", sizeof (Fiber));
#if 1
    for (int n = 0; n < 50; ++n) {
        for (int i = 0; i < 5; ++i)
            leds[i] = n % (i+2) == 0;

        printf("hello %4d %010u\n", systick::millis(), cycles::count());
        asm ("wfi");

        leds[5] = 0;
        asm ("wfi");
        leds[5] = 1;

        Device::processAllPending();
    }
#endif
    debugf("11\n");

    Fiber::app = []() {
        while (true)
            for (int i = 0; i < 5; ++i) {
                leds[i].toggle();
                Fiber::suspend(timers, 1000);
            }
    };

    while (true) {
        Fiber::runLoop();
        leds[5] = 0;
        asm ("wfi");
        leds[5] = 1;
    }
}
