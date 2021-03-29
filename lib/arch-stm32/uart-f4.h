struct Uart : Event {
    Uart (int n) : dev (findDev(uartInfo, n)) {
        if (dev.num == n) regHandler(); else dev.num = 0;
    }
    ~Uart () override { deinit(); }

    void init () {
        Periph::bitSet(RCC_APB1ENR, dev.ena);      // uart on
        Periph::bitSet(RCC_AHB1ENR, 21+dev.rxDma); // dma on

        uartMap[dev.pos] = this;

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
            uartMap[dev.pos] = nullptr;
        }
    }

    // install the uart IRQ dispatch handler in the hardware IRQ vector
    void installIrq (uint8_t irq) {
        nvicEnable(irq, dev.pos, []() {
            auto vecNum = MMIO8(0xE000ED04); // ICSR
            auto arg = irqArg[vecNum-0x10];
            uartMap[arg]->irqHandler();
        });
    }

    // the actual interrupt handler, with access to the uart object
    void irqHandler () {
        (uint32_t) devReg(SR); // read uart status register
        (uint32_t) devReg(DR); // clear idle interrupt and errors

        auto rx = dev.rxStream, tx = dev.txStream;
        uint8_t rxStat = dmaReg(LISR+(rx&~3)) >> 8*(rx&3);
        dmaReg(LIFCR+(rx&~3)) = rxStat << 8*(rx&3);
        uint8_t txStat = dmaReg(LISR+(tx&~3)) >> 8*(tx&3);
        dmaReg(LIFCR+(tx&~3)) = txStat << 8*(tx&3);

        if ((txStat & (1<<3)) != 0 && txFill > 0) // TCIF
            txStart(txNext, txFill);

        Stacklet::setPending(_id);
    }

    void baud (uint32_t rate, uint32_t hz) const {
        devReg(BRR) = (hz + rate/2) / rate;
    }

    auto rxFill () const -> uint16_t {
        return sizeof rxBuf - dmaRX(SNDTR);
    }

    void txStart (void const* ptr, uint16_t len) {
        dmaTX(SCR) &= ~1; // ~EN
        dmaTX(SNDTR) = len;
        dmaTX(SM0AR) = (uint32_t) ptr;
        dmaTX(SCR) |= 1; // EN
        txFill = 0;
    }

    DevInfo dev;
    void const* txNext;
    volatile uint16_t txFill = 0;
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
