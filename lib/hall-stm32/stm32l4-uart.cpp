#include "hall.h"

using namespace hall;
using namespace hall::dev;

struct DmaInfo { uint32_t base; uint8_t streams [8]; };

DmaInfo const dmaInfo [] = {
    { DMA1.addr, { 0, 11, 12, 13, 14, 15, 16, 17 }},
    { DMA2.addr, { 0, 56, 57, 58, 59, 60, 68, 69 }},
};

struct UartInfo {
    uint8_t num :4, ena, irq;
    uint16_t dma :1, rxChan :4, rxStream :3, txChan :4, txStream :3;
    uint32_t base;

    auto dmaBase () const { return dmaInfo[dma].base; }
};

struct Uart : Device {
    Uart (UartInfo const& d) : dev (d) {}

    auto& devReg (int off) const {
        return anyIo32(dev.base+off);
    }
    auto devReg (int off, int bit, uint8_t num =1) const {
        return anyIo32(dev.base+off, bit, num);
    }
    auto& dmaReg (int off) const {
        return anyIo32(dev.dmaBase()+off);
    }
    auto& dmaRX (int off) const {
        return anyIo32(dev.dmaBase()+off+0x14*(dev.rxStream-1));
    }
    auto dmaRX (int off, int bit, uint8_t num =1) const {
        return anyIo32(dev.dmaBase()+off+0x14*(dev.rxStream-1), bit, num);
    }
    auto& dmaTX (int off) const {
        return anyIo32(dev.dmaBase()+off+0x14*(dev.txStream-1));
    }
    auto dmaTX (int off, int bit, uint8_t num =1) const {
        return anyIo32(dev.dmaBase()+off+0x14*(dev.txStream-1), bit, num);
    }

    void init (char const* desc, uint32_t rate) {
        Pin::define(desc);
        rxBuf = pool.allocate();

        RCC(APB1ENR, dev.ena) = 1; // uart on
        RCC(AHB1ENR, dev.dma) = 1; // dma on

        dmaRX(CNDTR) = RXSIZE;
        dmaRX(CPAR) = dev.base + RDR;
        dmaRX(CMAR) = (uint32_t) rxBuf;
        dmaRX(CCR) = 0b1010'0111; // MINC CIRC HTIE TCIE EN

        dmaTX(CPAR) = dev.base + TDR;
        dmaTX(CMAR) = 0;
        dmaTX(CCR) = 0b1001'0000; // MINC DIR

        baud(rate);
        devReg(CR1) = 0b0101'1101; // TCIE IDLEIE TE RE UE
        devReg(CR3) = 0b1100'0000; // DMAT DMAR

        auto rxSh = 4*(dev.rxStream-1), txSh = 4*(dev.txStream-1);
        dmaReg(CSELR) = (dmaReg(CSELR) & ~(0xF<<rxSh) & ~(0xF<<txSh)) |
                                    (dev.rxChan<<rxSh) | (dev.txChan<<txSh);
        irqInstall(dev.irq);
        irqInstall(dmaInfo[dev.dma].streams[dev.rxStream]);
        irqInstall(dmaInfo[dev.dma].streams[dev.txStream]);
    }

    void deinit () {
        RCC(APB1ENR, dev.ena) = 0; // uart off
        RCC(AHB1ENR, dev.dma) = 0; // dma off
        pool.releasePtr(rxBuf); // TODO what about a TX in progress?
    }

    void baud (uint32_t bd, uint32_t hz =systemHz()) const {
        devReg(BRR) = (hz+bd/2)/bd;
    }

    auto rxNext () -> uint16_t { return RXSIZE - dmaRX(CNDTR); }

    void txStart (uint8_t i) {
        dmaTX(CNDTR) = pool.tag(i);
        dmaTX(CMAR) = (uint32_t) pool[i];
        devReg(CR) = (1<<6); // clear TC
        dmaTX(CCR, 0) = 1; // EN
    }

    // the actual interrupt handler, with access to the uart object
    void interrupt () override {
        if (devReg(SR) & (1<<4)) { // is this an rx-idle interrupt?
            auto next = rxNext();
            if (next >= 2 && rxBuf[next-1] == 0x03 && rxBuf[next-2] == 0x03)
                systemReset(); // two CTRL-C's in a row *and* idling: reset!
        }

        devReg(CR) = 0b0101'1111; // clear TCCF, IDLECF, and error flags

        auto rxSh = 4*(dev.rxStream-1);
        dmaReg(IFCR) = 1<<rxSh; // global clear RX DMA

        Device::interrupt();
    }

    void process () override {
        if (dmaTX(CCR, 0) && dmaTX(CNDTR) == 0) { // EN
            dmaTX(CCR, 0) = 0; // ~EN
            pool.releasePtr((uint8_t*)(uint32_t) dmaTX(CMAR));
            writers.post();
        }
    }

    void expire (uint16_t now, uint16_t& limit) {
        readers.expire(now, limit);
        writers.expire(now, limit);
    }

    UartInfo dev;
    Semaphore writers {1}, readers {0};
protected:
    constexpr static auto RXSIZE = pool.BUFLEN;
    uint8_t* rxBuf;
private:
    enum { AHB1ENR=0x48, APB1ENR=0x58 };
    enum { CR1=0x00,CR3=0x08,BRR=0x0C,SR=0x1C,CR=0x20,RDR=0x24,TDR=0x28 };
    enum { ISR=0x00,IFCR=0x04,CCR=0x08,CNDTR=0x0C,CPAR=0x10,CMAR=0x14,CSELR=0xA8 };
};

Uart uarts [] = {
    UartInfo { 1, 78, 37, 0, 2, 5, 2, 4, USART1.addr },
    UartInfo { 2, 17, 38, 0, 2, 6, 2, 7, USART2.addr },
    UartInfo { 3, 18, 38, 0, 2, 3, 2, 2, USART3.addr },
    UartInfo { 4, 19, 52, 1, 2, 5, 2, 3, UART4.addr  },
};

namespace hall::uart {
    void init (int n, char const* desc, int baud) {
        uarts[n-1].init(desc, baud);
    }

    void deinit (int n) {
        uarts[n-1].deinit();
    }

    auto getc (int n) -> int {
        (void) n; // TODO
        return 0;
    }

    void putc (int n, int c) {
        static uint8_t* buf;
        static uint8_t fill;

        auto flush = [&]() {
            auto i = pool.idOf(buf);
            pool.tag(i) = fill;
            uarts[n-1].writers.pend();
            uarts[n-1].txStart(i);
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
}
