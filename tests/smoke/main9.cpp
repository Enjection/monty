#include "boss.h"
#include "hall.h"

using namespace boss;
using namespace hall;

struct DmaInfo { uint32_t base; uint8_t streams [8]; };

DmaInfo const dmaInfo [] = {
    { dev::DMA1(0).addr, { 0, 11, 12, 13, 14, 15, 16, 17 }},
    { dev::DMA2(0).addr, { 0, 56, 57, 58, 59, 60, 68, 69 }},
};

struct UartInfo {
    uint8_t num :4, ena, irq;
    uint16_t dma :1, rxChan :4, rxStream :3, txChan :4, txStream :3;
    uint32_t base;

    auto dmaBase () const { return dmaInfo[dma].base; }
};

namespace hall {
    using namespace dev;
#include <uart-stm32l4.h>
}

Uart uart [] = {
    UartInfo { 1, 78, 37, 0, 2, 5, 2, 4, dev::USART1(0).addr },
    UartInfo { 2, 17, 38, 0, 2, 6, 2, 7, dev::USART2(0).addr },
    UartInfo { 3, 18, 38, 0, 2, 3, 2, 2, dev::USART3(0).addr },
    UartInfo { 4, 19, 52, 1, 2, 5, 2, 3, dev::UART4(0).addr  },
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

    if (fill >= pool.BUFLEN)
        flush();
    if (fill == 0)
        buf = pool.allocate();
    buf[fill++] = c;
    if (c == '\n')
        flush();
}

extern "C" int printf (const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    auto r = veprintf(uartPutc, &uart[1], fmt, ap);
    va_end(ap);
    return r;
}

void boss::debugf (const char*, ...) __attribute__((alias ("printf")));

void Fiber::processPending () {
    Device::dispatch();

    uint16_t limit = 100;
    timers.expire(systick::millis(), limit);
    systick::init(limit);
}

void Fiber::app () {

    for (int i = 0; true; ++i) {
        msWait(8);
        printf("> %*u /\n", 76 - (i % 70), systick::millis());
    }
}

int main () {
    fastClock();
    systick::init();
    uart[1].init("A2:PU7,A15:PU3", 115200);
    printf("abc\n");

    Pin led;
    led.config("B3:P");

    while (Fiber::runLoop()) {
        led = 0;
        idle();
        led = 1;
    }
}
