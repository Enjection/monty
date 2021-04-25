#include <hall.h>

using namespace hall;

struct DmaInfo { uint32_t base; uint8_t streams [8]; };

DmaInfo const dmaInfo [] = {
    { 0x4002'0000, { 0, 11, 12, 13, 14, 15, 16, 17 }},
    { 0x4002'0400, { 0, 56, 57, 58, 59, 60, 68, 69 }},
};

struct UartInfo {
    uint8_t pos :4, num :4, ena;
    uint8_t rxDma :1, rxChan :4, rxStream :3;
    uint8_t txDma :1, txChan :4, txStream :3;
    uint8_t irq;
    uint32_t base;

    auto dmaBase () const { return dmaInfo[rxDma].base; }

    void initIrqs() const {
        nvicEnable(irq);
        nvicEnable(dmaInfo[rxDma].streams[rxStream]);
        nvicEnable(dmaInfo[txDma].streams[txStream]);
    }
};

UartInfo const uartInfo [] = {
    { 0, 1, 78, 0, 2, 5, 0, 2, 4, 37, 0x4001'3800 },
    { 1, 2, 17, 0, 2, 6, 0, 2, 7, 38, 0x4000'4400 },
    { 2, 3, 18, 0, 2, 3, 0, 2, 2, 38, 0x4000'4800 },
    { 3, 4, 19, 1, 2, 5, 1, 2, 3, 52, 0x4000'4C00 },
};

namespace hall {
    struct Device {
        virtual void interrupt () {
            // TODO
        }
    };

#include <uart-stm32l4.h>
}

Uart uart {uartInfo[1]};

extern "C" {
    void USART2_IRQHandler () { uart.interrupt(); }
    void DMA1_Channel6_IRQHandler () __attribute__ ((alias ("USART2_IRQHandler")));
    void DMA1_Channel7_IRQHandler () __attribute__ ((alias ("USART2_IRQHandler")));
}

int main () {
    Pin leds [7];
    Pin::define("A6:P,A5,A4,A3,A1,A0,B3", leds, 7);

    systick::init(); // defaults to 100 ms

    Pin txrx [2];
    Pin::define("A2:PU7,A15:PU3", txrx, 2);
    uart.init();

    for (int n = 0; n < 50; ++n) {
        for (int i = 0; i < 6; ++i)
            leds[i] = n % (i+2) == 0;

        leds[6] = 0;
        asm ("wfi");
        leds[6] = 1;
    }
}
