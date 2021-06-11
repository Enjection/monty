struct Uart : Device {
    Uart (int n) : dev (findDev(uartInfo, n)) { if (dev.num != n) dev.num = 0; }
    ~Uart () override { deinit(); }

    void init () {
        Periph::bitSet(RCC_APB1ENR, dev.ena);      // uart on
        Periph::bitSet(RCC_AHB1ENR, 21+dev.rxDma); // dma on

        dmaRX(SNDTR) = sizeof rxBuf;
        dmaRX(SPAR) = dev.base + DR;
        dmaRX(SM0AR) = (uint32_t) rxBuf;
        dmaRX(SCR) = // CHSEL, MINC, CIRC, TCIE, HTIE, EN
            (dev.rxChan<<25) | 0b0101'0001'1001;

        dmaTX(SPAR) = dev.base + DR;
        dmaTX(SCR) = (dev.txChan<<25) | 0b0100'0101'0000; // CHSEL, MINC, DIR, TCIE

        devReg(CR1) = 0b0010'0000'0001'1100; // UE, IDLEIE, TE, RE
        devReg(CR3) = 0b1100'0000; // DMAT, DMAR

        installIrq((uint8_t) dev.irq);
        installIrq(dmaInfo[dev.rxDma].streams[dev.rxStream]);
        installIrq(dmaInfo[dev.txDma].streams[dev.txStream]);
    }

    void deinit () {
        if (dev.num > 0) {
            Periph::bitClear(RCC_APB1ENR, dev.ena);      // uart off
            Periph::bitClear(RCC_AHB1ENR, 21+dev.rxDma); // dma off
        }
    }

    // the actual interrupt handler, with access to the uart object
    void irqHandler () override {
        if (devReg(SR) & (1<<4)) { // is this an rx-idle interrupt?
            auto fill = rxFill();
            if (fill >= 2 && rxBuf[fill-1] == 0x03 && rxBuf[fill-2] == 0x03)
                reset(); // two CTRL-C's in a row *and* idling: reset!
        }

        (uint32_t) devReg(DR); // clear idle interrupt and errors

        auto rx = dev.rxStream, tx = dev.txStream;
        uint8_t rxStat = dmaReg(LISR+(rx&~3)) >> 8*(rx&3);
        dmaReg(LIFCR+(rx&~3)) = rxStat << 8*(rx&3);
        uint8_t txStat = dmaReg(LISR+(tx&~3)) >> 8*(tx&3);
        dmaReg(LIFCR+(tx&~3)) = txStat << 8*(tx&3);
/*
        if ((txStat & (1<<3)) != 0) { // TCIF
            txStart(txNext, txFill);
            txFill = 0;
        }
*/
        trigger();
    }

    void baud (uint32_t bd, uint32_t hz) const { devReg(BRR) = (hz/4+bd/2)/bd; }
    auto rxFill () const -> uint16_t { return sizeof rxBuf - dmaRX(SNDTR); }
    auto txBusy () const -> bool { return dmaTX(SNDTR) != 0; }

    void txStart (void const* ptr, uint16_t len) {
        if (len > 0) {
            dmaTX(SCR) &= ~1; // ~EN
            dmaTX(SNDTR) = len;
            dmaTX(SM0AR) = (uint32_t) ptr;
            dmaTX(SCR) |= 1; // EN
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
        return dmaReg(off+0x18*dev.rxStream);
    }
    auto dmaTX (int off) const -> volatile uint32_t& {
        return dmaReg(off+0x18*dev.txStream);
    }

    enum { SR=0x00,DR=0x04,BRR=0x08,CR1=0x0C,CR3=0x14 };
    enum { LISR=0x00,LIFCR=0x08,SCR=0x10,SNDTR=0x14,SPAR=0x18,SM0AR=0x1C };
};
