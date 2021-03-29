struct Uart : Event {
    Uart (int n) : dev (findDev(uartInfo, n)) {
        if (dev.num == n)
            regHandler();
        else
            dev.num = 0;
    }

    ~Uart () override { deinit(); }

    void init (uint8_t tx, uint8_t rx, uint32_t rate =115200) {
        assert(dev.num > 0);
        uartMap[dev.pos] = this;

        Periph::bitSet(RCC_APB1ENR+4*(dev.ena/32), dev.ena%32); // uart on
        Periph::bitSet(RCC_AHB1ENR, 21+dev.rxDma);              // dma on

        // TODO need a simpler way, still using JeeH pinmodes
        auto m = (int) Pinmode::alt_out;
        jeeh::Pin t ('A'+(tx>>4), tx&0x1F); t.mode(m, findAlt(altTX, tx, dev.num));
        jeeh::Pin r ('A'+(rx>>4), rx&0x1F); r.mode(m, findAlt(altRX, rx, dev.num));

        dmaRX(0x14) = sizeof rxBuf;     // SxNDTR
        dmaRX(0x18) = dev.base + dr;    // SxPAR
        dmaRX(0x1C) = (uint32_t) rxBuf; // SxM0AR
        dmaRX(0x10) = // SxCR: CHSEL, MINC, CIRC, TCIE, HTIE, EN
            (dev.rxChan<<25) | (1<<10) | (1<<8) | (1<<4) | (1<<3) | (1<<0);

        dmaTX(0x18) = dev.base + dr;    // SxPAR
        dmaTX(0x10) = // SxCR: CHSEL, MINC, DIR, TCIE
            (dev.txChan<<25) | (1<<10) | (1<<6) | (1<<4);

        baud(rate, F_CPU); // assume that the clock is running full speed

        MMIO32(dev.base+cr1) =
            (1<<13) | (1<<4) | (1<<3) | (1<<2); // UE, IDLEIE, TE, RE
        MMIO32(dev.base+cr3) = (1<<7) | (1<<6); // DMAT, DMAR

        installIrq((uint8_t) dev.irq, uartIrq, dev.pos);
        installIrq(dmaInfo[dev.rxDma].streams[dev.rxStream], uartIrq, dev.pos);
        installIrq(dmaInfo[dev.txDma].streams[dev.txStream], uartIrq, dev.pos);
    }

    void deinit () {
        if (dev.num == 0)
            return;
        Periph::bitClear(RCC_APB1ENR+4*(dev.ena/32), dev.ena%32); // uart off
        Periph::bitClear(RCC_AHB1ENR, 21+dev.rxDma);              // dma off
        uartMap[dev.pos] = nullptr; // TODO can there be a pending irq?
    }

    // this is installed as IRQ handler in the hardware IRQ vector
    static void uartIrq () {
        auto vecNum = MMIO8(0xE000ED04); // ICSR
        auto arg = irqArg[vecNum-0x10];
        uartMap[arg]->uartIrqHandler();
    }

    // the actual interrupt handler, with access to the object fields
    void uartIrqHandler () {
        (void) MMIO32(dev.base+sr); // read uart status register
        (void) MMIO32(dev.base+dr); // clear idle interrupt and errors

        auto rx = dev.rxStream;
        uint8_t rxStat = dmaBase(rx&~3) >> 8*(rx&3); // LISR or HISR
        dmaBase(0x08+(rx&~3)) = rxStat << 8*(rx&3);  // LIFCR or HIFCR

        auto tx = dev.txStream;
        uint8_t txStat = dmaBase(tx&~3) >> 8*(tx&3); // LISR or HISR
        dmaBase(0x08+(tx&~3)) = txStat << 8*(tx&3);  // LIFCR or HIFCR

        if ((txStat & (1<<3)) != 0 && txFill > 0)    // TCIF
            txStart(txNext, txFill);

        Stacklet::setPending(_id);
    }

    void baud (uint32_t rate, uint32_t hz =defaultHz) const {
        MMIO32(dev.base+brr) = (hz + rate/2) / rate;
    }

    auto rxFill () const -> uint16_t {
        return sizeof rxBuf - dmaRX(0x14); // SxNDTR
    }

    void txStart (void const* ptr, uint16_t len) {
        dmaTX(0x14) = len;            // SxNDTR
        dmaTX(0x1C) = (uint32_t) ptr; // SxM0AR
        dmaTX(0x10) |= (1<<0);        // EN
        txFill = 0;
    }

    DevInfo dev;
    void const* txNext;
    volatile uint16_t txFill = 0;
    uint8_t rxBuf [10]; // TODO volatile?
private:
    auto dmaBase (int off) const -> volatile uint32_t& {
        return MMIO32(dmaInfo[dev.rxDma].base + off);
    }
    auto dmaRX (int off) const -> volatile uint32_t& {
        return dmaBase(off + 0x18 * dev.rxStream);
    }
    auto dmaTX (int off) const -> volatile uint32_t& {
        return dmaBase(off + 0x18 * dev.txStream);
    }

    constexpr static auto sr  = 0x00; // status
    constexpr static auto dr  = 0x04; // data
    constexpr static auto brr = 0x08; // baud rate
    constexpr static auto cr1 = 0x0C; // control reg 1
    constexpr static auto cr3 = 0x14; // control reg 3
};
