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
        Periph::bitSet(RCC_AHB1ENR, dev.rxDma);                 // dma on

        // TODO need a simpler way, still using JeeH pinmodes
        auto m = (int) Pinmode::alt_out;
        jeeh::Pin t ('A'+(tx>>4), tx&0x1F); t.mode(m, findAlt(altTX, tx, dev.num));
        jeeh::Pin r ('A'+(rx>>4), rx&0x1F); r.mode(m, findAlt(altRX, rx, dev.num));

        auto rxSh = 4*(dev.rxStream-1), txSh = 4*(dev.txStream-1);
        dmaBase(0xA8) = (dmaBase(0xA8) & ~(0xF<<rxSh) & ~(0xF<<txSh)) |
                           (dev.rxChan<<rxSh) | (dev.txChan<<txSh);

        dmaRX(0x0C) = sizeof rxBuf;     // CNDTRx
        dmaRX(0x10) = dev.base + rdr;   // CPARx
        dmaRX(0x14) = (uint32_t) rxBuf; // CMARx
        dmaRX(0x08) = // CCRx: MINC, CIRC, HTIE, TCIE, EN
            (1<<7) | (1<<5) | (1<<2) | (1<<1) | (1<<0);

        dmaTX(0x10) = dev.base + tdr;   // CPARx
        dmaTX(0x08) = (1<<7) | (1<<4) | (1<<1); // CCRx: MINC, DIR, TCIE

        baud(rate, F_CPU); // assume that the clock is running full speed

        MMIO32(dev.base+cr1) = // IDLEIE, TE, RE, UE
            (1<<4) | (1<<3) | (1<<2) | (1<<0);
        MMIO32(dev.base+cr3) = (1<<7) | (1<<6); // DMAT, DMAR

        installIrq((uint8_t) dev.irq, uartIrq, dev.pos);
        installIrq(dmaInfo[dev.rxDma].streams[dev.rxStream], uartIrq, dev.pos);
        installIrq(dmaInfo[dev.txDma].streams[dev.txStream], uartIrq, dev.pos);
    }

    void deinit () {
        if (dev.num == 0)
            return;
        Periph::bitClear(RCC_APB1ENR+4*(dev.ena/32), dev.ena%32); // uart off
        Periph::bitClear(RCC_AHB1ENR, dev.rxDma);                 // dma off
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
        MMIO32(dev.base+icr) = 0x1F; // clear idle and error flags

        auto rxSh = 4*(dev.rxStream-1), txSh = 4*(dev.txStream-1);
        auto dmaIsr = dmaBase(0x00);
        dmaBase(0x04) = (1<<rxSh) | (1<<txSh); // global clear rx and tx dma

        if ((dmaIsr & (1<<(1+txSh))) != 0 && txFill > 0) // TCIF
            txStart(txNext, txFill);

        Stacklet::setPending(_id);
    }

    void baud (uint32_t rate, uint32_t hz =defaultHz) const {
        MMIO32(dev.base+brr) = (hz + rate/2) / rate;
    }

    auto rxFill () const -> uint16_t {
        return sizeof rxBuf - dmaRX(0x0C); // CNDTRx
    }

    void txStart (void const* ptr, uint16_t len) {
        dmaTX(0x08) &= ~(1<<0);       // EN
        dmaTX(0x0C) = len;            // CNDTRx
        dmaTX(0x14) = (uint32_t) ptr; // CMARx
        dmaTX(0x08) |= (1<<0);        // EN
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
        return dmaBase(off + dmaStep * (dev.rxStream - 1));
    }
    auto dmaTX (int off) const -> volatile uint32_t& {
        return dmaBase(off + dmaStep * (dev.txStream - 1));
    }

    constexpr static auto cr1 = 0x00; // control reg 1
    constexpr static auto cr3 = 0x08; // control reg 3
    constexpr static auto brr = 0x0C; // baud rate
    constexpr static auto isr = 0x1C; // interrupt status
    constexpr static auto icr = 0x20; // interrupt clear
    constexpr static auto rdr = 0x24; // recv data
    constexpr static auto tdr = 0x28; // xmit data
    constexpr static auto dmaStep = 0x14;
};
