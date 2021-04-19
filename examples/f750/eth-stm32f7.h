struct Eth : Interface {
    auto operator new (size_t sz) -> void* { return allocateNonCached(sz); }

    Eth (MacAddr const& mac) : Interface (mac) {}

    // FIXME RCC name clash mcb/device
    constexpr static auto RCC    = io32<0x4002'3800>;
    constexpr static auto SYSCFG = io32<device::SYSCFG>;

    constexpr static auto DMA = io32<ETHERNET_DMA>;
    enum { BMR=0x00,TPDR=0x04,RPDR=0x08,RDLAR=0x0C,TDLAR=0x10,
           DSR=0x14,OMR=0x18,IER=0x1C };

    constexpr static auto MAC = io32<ETHERNET_MAC>;
    enum { CR=0x00,FFR=0x04,HTHR=0x08,HTLR=0x0C,MIIAR=0x10,MIIDR=0x14,
           FCR=0x18,MSR=0x38,IMR=0x3C,A0HR=0x40,A0LR=0x44 };

    auto readPhy (int reg) -> uint16_t {
        MAC(MIIAR) = (0<<11)|(reg<<6)|(0b100<<2)|(1<<0);
        while (MAC(MIIAR)[0] == 1) {} // wait until MB clear
        return MAC(MIIDR);
    }
    void writePhy (int reg, uint16_t val) {
        MAC(MIIDR) = val;
        MAC(MIIAR) = (0<<11)|(reg<<6)|(0b100<<2)|(1<<1)|(1<<0);
        while (MAC(MIIAR)[0] == 1) {} // wait until MB clear
    }

    struct DmaDesc {
        int32_t volatile stat;
        uint32_t size;
        uint8_t* data;
        DmaDesc* next;
        uint32_t extStat, _gap, times [2];

        auto available () const { return stat >= 0; }

        auto release () {
            asm volatile ("dsb"); stat |= (1<<31); asm volatile ("dsb");
            return next;
        }
    };

    constexpr static auto NRX = 5, NTX = 5, BUFSZ = 1524;
    DmaDesc rxDesc [NRX], txDesc [NTX];
    DmaDesc *rxNext = rxDesc, *txNext = txDesc;
    uint8_t rxBufs [NRX][BUFSZ+4], txBufs [NTX][BUFSZ+4];

    void init () {
        // F7508-DK pins, all using alt mode 11:
        //      A1: refclk, A2: mdio, A7: crsdiv, C1: mdc, C4: rxd0,
        //      C5: rxd1, G2: rxer, G11: txen, G13: txd0, G14: txd1,
        mcu::Pin pins [10];
        Pin::define("A1:PV11,A2,A7,C1,C4,C5,G2,G11,G13,G14", pins, sizeof pins);

        RCC(0x30).mask(25, 3) = 0b111; // ETHMAC EN,RXEN,TXEN in AHB1ENR

        RCC(0x44)[14] = 1; // SYSCFGEN in APB2ENR
        SYSCFG(0x04)[23] = 1; // RMII_SEL in PMC

        DMA(BMR)[0] = 1; // SR in DMABMR
        while (DMA(BMR)[0] != 0) {}

        writePhy(0, 0x8000); // PHY reset
        msWait(250); // long
auto t = millis();
        while ((readPhy(1) & (1<<2)) == 0) // wait for link
            msWait(1); // keep updating the ticks
t = millis() - t;
        writePhy(0, 0x1000); // PHY auto-negotiation
        while ((readPhy(1) & (1<<5)) == 0) {} // wait for a-n complete
        auto r = readPhy(31);
        auto duplex = (r>>4) & 1, fast = (r>>3) & 1;
printf("link %d ms full-duplex %d 100-Mbit/s %d\n", t, duplex, fast);

        MAC(CR) = (1<<15) | (fast<<14) | (duplex<<11) | (1<<10); // IPCO
        msWait(1);

        // not set: MAC(FFR) MAC(HTHR) MAC(HTLR) MAC(FCR)

        DMA(BMR) = // AAB USP RDP FB PM PBL EDFE DA
            (1<<25)|(1<<24)|(1<<23)|(32<<17)|(1<<16)|(1<<14)|(32<<8)|(1<<7)|(1<<1);
        msWait(1);

        MAC(A0HR) = ((uint16_t const*) &_mac)[2];
        MAC(A0LR) = ((uint32_t const*) &_mac)[0];

        for (int i = 0; i < NTX; ++i) {
            txDesc[i].stat = 0; // not owned by DMA
            txDesc[i].data = txBufs[i] + 2;
            txDesc[i].next = txDesc + (i+1) % NTX;
        }

        for (int i = 0; i < NRX; ++i) {
            rxDesc[i].stat = (1<<31); // OWN
            rxDesc[i].size = (1<<14) | BUFSZ; // RCH SIZE
            rxDesc[i].data = rxBufs[i] + 2;
            rxDesc[i].next = rxDesc + (i+1) % NRX;
        }

        DMA(TDLAR) = (uint32_t) txDesc;
        DMA(RDLAR) = (uint32_t) rxDesc;

        MAC(CR)[3] = 1; msWait(1); // TE
        MAC(CR)[2] = 1; msWait(1); // RE

        DMA(OMR) = (1<<21) | (1<<20) | (1<<13) | (1<<1); // TSF FTF ST SR

        MAC(IMR) = (1<<9) | (1<<3); // TSTIM PMTIM
        DMA(IER)= (1<<16) | (1<<6) | (1<<0); // NISE RIE TIE

        irqInstall((uint8_t) IrqVec::ETH);
    }

    void irqHandler () override {
//printf("E! msr %08x dsr %08x\n", (uint32_t) MAC(MSR), (uint32_t) DMA(DSR));
        DMA(DSR) = (1<<16) | (1<<6) | (1<<0); // clear NIS RS TS

        trigger();
    }

    void poll () {
        while (rxNext->available()) {
            auto f = (Frame*) rxNext->data;
            f->received(*this);
            rxNext = rxNext->release();
            DMA(RPDR) = 0; // resume DMA
        }
    }

    auto recv () -> Chunk override {
        // TODO not used yet, process ARP/ICMP/DHCP, return only UDP & TCP
        while (!rxNext->available()) {
            msWait(1); // TODO suspend
        }
        return { rxNext->data, BUFSZ };
    }

    void didRecv (uint32_t n) override {
        // TODO not used yet
        ensure(sizeof (Frame) <= n && n <= BUFSZ);
        ensure(rxNext->available());
        rxNext = rxNext->release();
        DMA(RPDR) = 0; // resume DMA
    }

    auto canSend () -> Chunk override {
        while (!txNext->available()) {
            poll(); // keep processing incoming while waiting
            msWait(1); // TODO suspend
        }
        return { txNext->data, BUFSZ };
    }

    void send (uint32_t n) override {
        ensure(txNext->available());
        ensure(sizeof (Frame) <= n && n <= BUFSZ);
        txNext->size = n;
        txNext->stat = (0b0111<<28) | (3<<22) | (1<<20); // IC LS FS CIC TCH
        txNext = txNext->release();
        DMA(TPDR) = 0; // resume DMA
    }
};
