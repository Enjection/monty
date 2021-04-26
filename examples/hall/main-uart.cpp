#include <hall.h>

using namespace hall;

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

int main () {
    fastClock();

    Pin leds [7];
    Pin::define("A6:P,A5,A4,A3,A1,A0,B3", leds, 7);

    systick::init(); // defaults to 100 ms
    uart[1].init("A2:PU7,A15:PU3", 921600);

    for (int n = 0; n < 50; ++n) {
        for (int i = 0; i < 6; ++i)
            leds[i] = n % (i+2) == 0;

        leds[6] = 0;
        asm ("wfi");
        leds[6] = 1;
    }
}
