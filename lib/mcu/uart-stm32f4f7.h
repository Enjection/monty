struct Uart : Device {
    constexpr static auto AHB1ENR = 0x30;
    constexpr static auto APB1ENR = 0x40;

    Uart (int n) : dev (findDev(uartInfo, n)) { if (dev.num != n) dev.num = 0; }
    ~Uart () { deinit(); }

    void init () {
        RCC(APB1ENR)[dev.ena] = 1;      // uart on
        RCC(AHB1ENR)[21+dev.rxDma] = 1; // dma on

        dmaRX(SNDTR) = sizeof rxBuf;
        dmaRX(SPAR) = dev.base + RDR;
        dmaRX(SM0AR) = (uint32_t) rxBuf;
        dmaRX(SCR) = // CHSEL MINC CIRC TCIE HTIE EN
            (dev.rxChan<<25) | 0b0101'0001'1001;

        dmaTX(SPAR) = dev.base + TDR;
        dmaTX(SCR) = (dev.txChan<<25) | 0b0100'0101'0000; // CHSEL MINC DIR TCIE

        devReg(CR1) = FAMILY == STM_F4 ? // IDLEIE TE RE UE, different bits
                0b0010'0000'0001'1100 : 0b0001'1101;
        devReg(CR3) = 0b1100'0000; // DMAT DMAR

        irqInstall((uint8_t) dev.irq);
        irqInstall(dmaInfo[dev.rxDma].streams[dev.rxStream]);
        irqInstall(dmaInfo[dev.txDma].streams[dev.txStream]);
    }

    void deinit () {
        if (dev.num > 0) {
            RCC(APB1ENR)[dev.ena] = 0;      // uart off
            RCC(AHB1ENR)[21+dev.rxDma] = 0; // dma off
        }
    }

    void baud (uint32_t bd, uint32_t hz) const { devReg(BRR) = (hz+bd/2)/bd; }
    auto rxNext () -> uint16_t { return sizeof rxBuf - dmaRX(SNDTR); }
    auto txLeft () -> uint16_t { return dmaTX(SNDTR); }

    void txStart () {
        auto len = txNext >= txLast ? txNext - txLast : sizeof txBuf - txLast;
        if (len > 0 && dmaTX(SCR)[0] == 0) {
            dmaTX(SNDTR) = len;
            dmaTX(SM0AR) = (uint32_t) txBuf + txLast;
            txLast = txWrap(txLast + len);
            //while (devReg(SR)[7] == 0) {} // wait for TXE
            dmaTX(SCR)[0] = 1; // EN
        }
    }

    DevInfo dev;
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
        return io32<0>(dmaInfo[dev.rxDma].base+off);
    }
    auto dmaRX (int off) const -> IOWord {
        return dmaReg(off+0x18*dev.rxStream);
    }
    auto dmaTX (int off) const -> IOWord {
        return dmaReg(off+0x18*dev.txStream);
    }

    // the actual interrupt handler, with access to the uart object
    void irqHandler () {
        if (devReg(SR) & (1<<4)) { // is this an rx-idle interrupt?
            auto next = rxNext();
            if (next >= 2 && rxBuf[next-1] == 0x03 && rxBuf[next-2] == 0x03)
                systemReset(); // two CTRL-C's in a row *and* idling: reset!
        }

        if constexpr (FAMILY == STM_F4)
            (uint32_t) devReg(RDR); // clear idle and error flags
        else // F7
            devReg(CR) = 0b0001'1111; // clear idle and error flags

        auto rx = dev.rxStream, tx = dev.txStream;
        uint8_t rxStat = dmaReg(LISR+(rx&~3)) >> 8*(rx&3);
        dmaReg(LIFCR+(rx&~3)) = rxStat << 8*(rx&3);
        uint8_t txStat = dmaReg(LISR+(tx&~3)) >> 8*(tx&3);
        dmaReg(LIFCR+(tx&~3)) = txStat << 8*(tx&3);

        if ((txStat & (1<<3)) != 0) // TCIF
            txStart();

        trigger();
    }

#if STM32F4
    enum { SR=0x00,RDR=0x04,TDR=0x04,BRR=0x08,CR1=0x0C,CR3=0x14 };
#elif STM32F7
    enum { CR1=0x00,CR3=0x08,BRR=0x0C,SR=0x1C,CR=0x20,RDR=0x24,TDR=0x28 };
#endif
    enum { LISR=0x00,LIFCR=0x08,SCR=0x10,SNDTR=0x14,SPAR=0x18,SM0AR=0x1C };
};
