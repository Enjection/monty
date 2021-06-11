struct Uart : Device {
    Uart (int n) : dev (findDev(uartInfo, n)) { if (dev.num != n) dev.num = 0; }
    ~Uart () override { deinit(); }

    void init () {
        Periph::bitSet(RCC_APB2ENR, dev.ena);   // uart on
        Periph::bitSet(RCC_AHBENR, dev.rxDma);  // dma on

        dmaRX(CNDTR) = sizeof rxBuf;
        dmaRX(CPAR) = dev.base + DR;
        dmaRX(CMAR) = (uint32_t) rxBuf;
        dmaRX(CCR) = 0b1010'0111; // MINC, CIRC, HTIE, TCIE, EN

        dmaTX(CPAR) = dev.base + DR;
        dmaTX(CCR) = 0b1001'0011; // MINC, DIR, TCIE, EN

        devReg(CR1) = 0b0010'0000'0001'1100; // UE, IDLEIE, TE, RE
        devReg(CR3) = 0b1100'0000; // DMAT, DMAR

        installIrq((uint8_t) dev.irq);
        installIrq(dmaInfo[dev.rxDma].streams[dev.rxStream]);
        installIrq(dmaInfo[dev.txDma].streams[dev.txStream]);
    }

    void deinit () {
        if (dev.num > 0) {
            Periph::bitClear(RCC_APB2ENR, dev.ena);   // uart off
            Periph::bitClear(RCC_AHBENR, dev.rxDma);  // dma off
        }
    }

    // the actual interrupt handler, with access to the uart object
    void irqHandler () {
        if (devReg(SR) & (1<<4)) { // is this an rx-idle interrupt?
            auto fill = rxFill();
            if (fill >= 2 && rxBuf[fill-1] == 0x03 && rxBuf[fill-2] == 0x03)
                reset(); // two CTRL-C's in a row *and* idling: reset!
        }

        (uint32_t) devReg(DR); // clear idle interrupt and errors

        auto rxSh = 4*(dev.rxStream-1), txSh = 4*(dev.txStream-1);
//      auto stat = dmaReg(ISR);
        dmaReg(IFCR) = (1<<rxSh) | (1<<txSh); // global clear rx and tx dma
/*
        if ((stat & (1<<(1+txSh))) != 0) { // TCIF
            txStart(txNext, txFill);
            txFill = 0;
        }
*/
        trigger();
    }

    void baud (uint32_t bd, uint32_t hz) const { devReg(BRR) = (hz+bd/2)/bd; }
    auto rxFill () const -> uint16_t { return sizeof rxBuf - dmaRX(CNDTR); }
    auto txBusy () const -> bool { return dmaTX(CNDTR) != 0; }

    void txStart (void const* ptr, uint16_t len) {
        if (len > 0) {
            dmaTX(CCR) &= ~1; // ~EN
            dmaTX(CNDTR) = len;
            dmaTX(CMAR) = (uint32_t) ptr;
            dmaTX(CCR) |= 1; // EN
        }
    }

    DevInfo dev;
//  void const* txNext;
//  volatile uint16_t txFill = 0;
    uint8_t rxBuf [100];
private:
    auto devReg (int off) const -> volatile uint32_t& {
        return MMIO32(dev.base+off);
    }
    auto dmaReg (int off) const -> volatile uint32_t& {
        return MMIO32(dmaInfo[dev.rxDma].base+off);
    }
    auto dmaRX (int off) const -> volatile uint32_t& {
        return dmaReg(off+0x20*(dev.rxStream-1));
    }
    auto dmaTX (int off) const -> volatile uint32_t& {
        return dmaReg(off+0x20*(dev.txStream-1));
    }

    enum { SR=0x00,DR=0x04,BRR=0x08,CR1=0x0C,CR3=0x14 };
    enum { ISR=0x00,IFCR=0x04,CCR=0x08,CNDTR=0x0C,CPAR=0x10,CMAR=0x14 };
};
