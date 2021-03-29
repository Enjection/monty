struct Uart : Event {
    Uart (int n) : dev (findDev(uartInfo, n)) {
        if (dev.num == n)
            regHandler();
        else
            dev.num = 0;
    }

    ~Uart () override { deinit(); }

    void init (uint8_t tx, uint8_t rx, uint32_t rate =115200) {
        uartMap[dev.pos] = this;

        Periph::bitSet(RCC_APB1ENR+4*(dev.ena/32), dev.ena%32); // uart on
        Periph::bitSet(RCC_AHB1ENR, dev.rxDma);                 // dma on

        // TODO need a simpler way, still using JeeH pinmodes
        auto m = (int) Pinmode::alt_out;
        jeeh::Pin t ('A'+(tx>>4), tx&0x1F); t.mode(m, findAlt(altTX, tx, dev.num));
        jeeh::Pin r ('A'+(rx>>4), rx&0x1F); r.mode(m, findAlt(altRX, rx, dev.num));

        auto rxSh = 4*(dev.rxStream-1), txSh = 4*(dev.txStream-1);
        dmaReg(CSELR) = (dmaReg(CSELR) & ~(0xF<<rxSh) & ~(0xF<<txSh)) |
                           (dev.rxChan<<rxSh) | (dev.txChan<<txSh);

        dmaRX(CNDTR) = sizeof rxBuf;
        dmaRX(CPAR) = dev.base + RDR;
        dmaRX(CMAR) = (uint32_t) rxBuf;
        dmaRX(CCR) = 0b1010'0111; // MINC, CIRC, HTIE, TCIE, EN

        dmaTX(CPAR) = dev.base + TDR;
        dmaTX(CCR) = 0b1001'0001; // MINC, DIR, TCIE

        baud(rate, F_CPU); // assume that the clock is running full speed

        devReg(CR1) = 0b0001'1101; // IDLEIE, TE, RE, UE
        devReg(CR3) = 0b1100'0000; // DMAT, DMAR

        installIrq((uint8_t) dev.irq);
        installIrq(dmaInfo[dev.rxDma].streams[dev.rxStream]);
        installIrq(dmaInfo[dev.txDma].streams[dev.txStream]);
    }

    void deinit () {
        if (dev.num == 0)
            return;
        Periph::bitClear(RCC_APB1ENR+4*(dev.ena/32), dev.ena%32); // uart off
        Periph::bitClear(RCC_AHB1ENR, dev.rxDma);                 // dma off
        uartMap[dev.pos] = nullptr; // TODO can there be a pending irq?
    }

    // install the uart IRQ dispatch handler in the hardware IRQ vector
    void installIrq (uint8_t irq) {
        irqArg[irq] = dev.pos;
        (&VTableRam().wwdg)[irq] = []() {
            auto vecNum = MMIO8(0xE000ED04); // ICSR
            auto arg = irqArg[vecNum-0x10];
            uartMap[arg]->uartIrqHandler();
        };
        nvicEnable(irq);
    }

    // the actual interrupt handler, with access to the uart object
    void uartIrqHandler () {
        devReg(ICR) = 0b0001'1111; // clear idle and error flags

        auto rxSh = 4*(dev.rxStream-1), txSh = 4*(dev.txStream-1);
        auto dmaIsr = dmaReg(ISR);
        dmaReg(IFCR) = (1<<rxSh) | (1<<txSh); // global clear rx and tx dma

        if ((dmaIsr & (1<<(1+txSh))) != 0 && txFill > 0) // TCIF
            txStart(txNext, txFill);

        Stacklet::setPending(_id);
    }

    void baud (uint32_t rate, uint32_t hz =defaultHz) const {
        devReg(BRR) = (hz + rate/2) / rate;
    }

    auto rxFill () const -> uint16_t {
        return sizeof rxBuf - dmaRX(CNDTR);
    }

    void txStart (void const* ptr, uint16_t len) {
        dmaTX(CCR) &= ~1; // ~EN
        dmaTX(CNDTR) = len;
        dmaTX(CMAR) = (uint32_t) ptr;
        dmaTX(CCR) |= 1; // EN
        txFill = 0;
    }

    DevInfo dev;
    void const* txNext;
    volatile uint16_t txFill = 0;
    uint8_t rxBuf [10]; // TODO volatile?
private:
    auto devReg (int off) const -> volatile uint32_t& {
        return MMIO32(dev.base+off);
    }
    auto dmaReg (int off) const -> volatile uint32_t& {
        return MMIO32(dmaInfo[dev.rxDma].base+off);
    }
    auto dmaRX (int off) const -> volatile uint32_t& {
        return dmaReg(off+0x14*(dev.rxStream-1));
    }
    auto dmaTX (int off) const -> volatile uint32_t& {
        return dmaReg(off+0x14*(dev.txStream-1));
    }

    enum { CR1=0x00,CR3=0x08,BRR=0x0C,SR=0x1C,ICR=0x20,RDR=0x24,TDR=0x28 };
    enum { ISR=0x00,IFCR=0x04,CCR=0x08,CNDTR=0x0C,CPAR=0x10,CMAR=0x14,CSELR=0xA8 };
};
