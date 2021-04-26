struct Uart : Device {
    Uart (UartInfo const& d) : dev (d) {}

    void init (char const* desc, uint32_t rate =115200) {
        Pin::define(desc);

        RCC(APB1ENR)[dev.ena] = 1; // uart on
        RCC(AHB1ENR)[dev.dma] = 1; // dma on

        dmaRX(CNDTR) = sizeof rxBuf;
        dmaRX(CPAR) = dev.base + RDR;
        dmaRX(CMAR) = (uint32_t) rxBuf;
        dmaRX(CCR) = 0b1010'0111; // MINC, CIRC, HTIE, TCIE, EN

        dmaTX(CPAR) = dev.base + TDR;
        dmaTX(CCR) = 0b1001'0011; // MINC, DIR, TCIE, EN

        baud(rate);
        devReg(CR1) = 0b0001'1101; // IDLEIE, TE, RE, UE
        devReg(CR3) = 0b1100'0000; // DMAT, DMAR

        auto rxSh = 4*(dev.rxStream-1), txSh = 4*(dev.txStream-1);
        dmaReg(CSELR) = (dmaReg(CSELR) & ~(0xF<<rxSh) & ~(0xF<<txSh)) |
                                    (dev.rxChan<<rxSh) | (dev.txChan<<txSh);
        irqInstall(dev.irq);
        irqInstall(dmaInfo[dev.dma].streams[dev.rxStream]);
        irqInstall(dmaInfo[dev.dma].streams[dev.txStream]);

txBuf[0] = 'a'; txBuf[1] = 'b'; txBuf[2] = 'c'; txNext = 3;
txStart();
    }

    void deinit () {
        RCC(APB1ENR)[dev.ena] = 0; // uart off
        RCC(AHB1ENR)[dev.dma] = 0; // dma off
    }

    void baud (uint32_t bd, uint32_t hz =systemHz()) const {
        devReg(BRR) = (hz+bd/2)/bd;
    }

    auto rxNext () -> uint16_t { return sizeof rxBuf - dmaRX(CNDTR); }
    auto txLeft () -> uint16_t { return dmaTX(CNDTR); }

    void txStart () {
        auto len = txNext >= txLast ? txNext - txLast : sizeof txBuf - txLast;
        if (len > 0) {
            dmaTX(CCR)[0] = 0; // ~EN
            dmaTX(CNDTR) = len;
            dmaTX(CMAR) = (uint32_t) txBuf + txLast;
            txLast = txWrap(txLast + len);
            while (devReg(SR)[7] == 0) {} // wait for TXE, TODO inside irq?
            dmaTX(CCR)[0] = 1; // EN
        }
    }

    // the actual interrupt handler, with access to the uart object
    void interrupt () override {
        if (devReg(SR) & (1<<4)) { // is this an rx-idle interrupt?
            auto next = rxNext();
            if (next >= 2 && rxBuf[next-1] == 0x03 && rxBuf[next-2] == 0x03)
                systemReset(); // two CTRL-C's in a row *and* idling: reset!
        }

        devReg(CR) = 0b0001'1111; // clear idle and error flags

        auto rxSh = 4*(dev.rxStream-1), txSh = 4*(dev.txStream-1);
        auto stat = dmaReg(ISR);
        dmaReg(IFCR) = (1<<rxSh) | (1<<txSh); // global clear rx and tx dma

        if ((stat & (1<<(1+txSh))) != 0) // TCIF
            txStart();

        Device::interrupt();
    }

    UartInfo dev;
protected:
    uint8_t rxBuf [100], txBuf [100];
    uint16_t txNext =0, txLast =0;

    static auto txWrap (uint16_t n) -> uint16_t {
        return n < sizeof txBuf ? n : n - sizeof txBuf;
    }
private:
    auto devReg (int off) const -> IOWord {
        return io32<0>(dev.base+off);
    }
    auto dmaReg (int off) const -> IOWord {
        return io32<0>(dev.dmaBase()+off);
    }
    auto dmaRX (int off) const -> IOWord {
        return dmaReg(off+0x14*(dev.rxStream-1));
    }
    auto dmaTX (int off) const -> IOWord {
        return dmaReg(off+0x14*(dev.txStream-1));
    }

    enum { AHB1ENR=0x48, APB1ENR=0x58 };
    enum { CR1=0x00,CR3=0x08,BRR=0x0C,SR=0x1C,CR=0x20,RDR=0x24,TDR=0x28 };
    enum { ISR=0x00,IFCR=0x04,CCR=0x08,CNDTR=0x0C,CPAR=0x10,CMAR=0x14,CSELR=0xA8 };
};
