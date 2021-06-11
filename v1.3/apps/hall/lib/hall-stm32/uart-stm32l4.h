struct Uart : Device {
    Uart (UartInfo const& d) : dev (d) {}

    void init (char const* desc, uint32_t rate) {
        Pin::define(desc);
        rxBuf = pool.allocate();

        RCC(APB1ENR)[dev.ena] = 1; // uart on
        RCC(AHB1ENR)[dev.dma] = 1; // dma on

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
        RCC(APB1ENR)[dev.ena] = 0; // uart off
        RCC(AHB1ENR)[dev.dma] = 0; // dma off
        pool.releasePtr(rxBuf); // TODO what about a TX in progress?
    }

    void baud (uint32_t bd, uint32_t hz =systemHz()) const {
        devReg(BRR) = (hz+bd/2)/bd;
    }

    auto rxNext () -> uint16_t { return RXSIZE - dmaRX(CNDTR); }
    auto txLeft () -> uint16_t { return dmaTX(CNDTR); }

    void txStart (uint8_t i) {
        dmaTX(CNDTR) = pool.tag(i) + 1;
        dmaTX(CMAR) = (uint32_t) pool[i];
        devReg(CR) = (1<<6); // clear TC
        dmaTX(CCR)[0] = 1; // EN
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
        if (dmaTX(CCR)[0]) { // EN
            dmaTX(CCR)[0] = 0; // ~EN
            pool.releasePtr((uint8_t*)(uint32_t) dmaTX(CMAR));
            //TODO tx queue post
        }
    }

    void expire (uint16_t now, uint16_t& limit) {
        readers.expire(now, limit);
        writers.expire(now, limit);
    }

    UartInfo dev;
protected:
    constexpr static auto RXSIZE = pool.SZBUF;
    Semaphore writers {1}, readers {0};
    uint8_t* rxBuf;
private:
    auto devReg (int off) const -> IOWord {
        return IOWord {dev.base+off};
    }
    auto dmaReg (int off) const -> IOWord {
        return IOWord {dev.dmaBase()+off};
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
