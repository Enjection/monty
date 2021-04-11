#include <mcu.h>

#include <cstdarg>
#include <printer.h>

using namespace mcu;

void spifTest (bool wipe =false) {
    // QSPI flash on F7508-DK:
    //  PD11 IO0 MOSI, PD12 IO1 MISO,
    //  PE2 IO2 (high), PD13 IO3 (high),
    //  PB2 SCLK, PB6 NSEL

    mcu::Pin dummy;
    dummy.define("E2:U");  // set io2 high
    dummy.define("D13:U"); // set io3 high

    SpiFlash spif;
    spif.init("D11,D12,B2,B6");
    spif.reset();

    printf("spif %x, size %d kB\n", spif.devId(), spif.size());

    msWait(1);
    auto t = millis();
    if (wipe) {
        printf("wipe ... ");
        spif.wipe();
    } else {
        printf("erase ... ");
        spif.erase(0x1000);
    }
    printf("%d ms\n", millis() - t);

    uint8_t buf [16];

    spif.read(0x1000, buf, sizeof buf);
    for (auto e : buf) printf(" %02x", e);
    printf(" %s", "\n");

    spif.write(0x1000, (uint8_t const*) "abc", 3);

    spif.read(0x1000, buf, sizeof buf);
    for (auto e : buf) printf(" %02x", e);
    printf(" %s", "\n");

    spif.write(0x1000, (uint8_t const*) "ABCDE", 5);

    spif.read(0x1000, buf, sizeof buf);
    for (auto e : buf) printf(" %02x", e);
    printf(" %s", "\n");
}

namespace qspi {
    // FIXME RCC name clash mcb/device
    constexpr auto RCC = io32<0x4002'3800>;

    void init (char const* defs, int fsize =24, int dummy =9) {
        mcu::Pin pins [6];
        Pin::define(defs, pins, 6);

        RCC(0x38)[1] = 1; // QSPIEN in AHB3ENR

        // see STM32F412 ref man: RM0402 rev 6, section 12, p.288 (QUADSPI)
        enum { CR=0x00, DCR=0x04, CCR=0x14 };

        // auto fsize = 24; // flash has 16 MB, 2^24 bytes
        // auto dummy = 9;  // number of cycles between cmd and data

        QSPI(CR) = (1<<4) | (1<<0);           // SSHIFT EN
        QSPI(DCR) = ((fsize-1)<<16) | (2<<8); // FSIZE CSHT

        // mem-mapped mode: FMODE DMODE DCYC ABMODE ADSIZE ADMODE INSTR
        QSPI(CCR) = (3<<26) | (3<<24) | (dummy<<18) | (3<<14) |
                    (2<<12) | (3<<10) | (1<<8) | (0xEB<<0);
    }

    void deinit () {
        RCC(0x38)[1] = 0; // ~QSPIEN in AHB3ENR
        // RCC(0x18)[1] = 1; RCC(0x18)[1] = 0; // toggle QSPIRST in AHB3RSTR
    }
};

void qspiTest () {
    qspi::init("D11:PV9,D12:PV9,E2:PV9,D13:PV9,B2:PV9,B6:PV10");

    auto qmem = (uint32_t const*) 0x90000000;
    for (int i = 0; i < 4; ++i) {
        auto v = qmem[0x1000/4+i];
        printf(" %08x", v);
    }
    printf(" %s", "\n");
}

namespace lcd {
#include "lcd.h"
}

static lcd::FrameBuffer<1> bg;
static lcd::FrameBuffer<2> fg;

void lcdTest () {
    lcd::init();
    bg.init();
    fg.init();

    for (int y = 0; y < lcd::HEIGHT; ++y)
        for (int x = 0; x < lcd::WIDTH; ++x)
            bg(x, y) = x ^ y;

    for (int y = 0; y < lcd::HEIGHT; ++y)
        for (int x = 0; x < lcd::WIDTH; ++x)
            fg(x, y) = ((16*x)/lcd::WIDTH << 4) | (y >> 4);
}

namespace eth {
    // FIXME RCC name clash mcb/device
    constexpr auto RCC    = io32<0x4002'3800>;
    constexpr auto SYSCFG = io32<device::SYSCFG>;

    constexpr auto DMA    = io32<ETHERNET_DMA>;
    enum { BMR=0x00,RDLAR=0x0C,TDLAR=0x10,ASR=0x14,OMR=0x18 };
    constexpr auto MAC    = io32<ETHERNET_MAC>;
    enum { MCR=0x00,FFR=0x04,HTHR=0x08,HTLR=0x0C,MIIAR=0x10,MIIDR=0x14,FCR=0x18 };

    struct DmaDesc {
        uint32_t volatile stat;
        uint32_t size;
        uint8_t* data;
        DmaDesc* next;
        uint32_t extStat, _gap, times [2];
    };

    constexpr auto NRX = 3, NTX = 3, BUFSZ = 1524;
    DmaDesc rxDesc [NRX], txDesc [NTX];
    uint8_t rxBufs [NRX][BUFSZ], txBufs [NTX][BUFSZ];

    uint8_t macAddr [] = {0x11,0x22,0x33,0x44,0x55,0x66};

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

    void init () {
debugf("eth init\n");
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

        //MAC(0x10).mask(2, 3) = 0b100; // CR in MAC
        writePhy(0, 0x8000); // PHY reset
        msWait(250); // value?
debugf("11 %d ms\n", millis());
        while ((readPhy(1) & (1<<2)) == 0) // wait for link
            msWait(1); // keep updating the ticks
debugf("22 %d ms\n", millis());
        writePhy(0, 0x1000); // PHY auto-negotiation
        while ((readPhy(1) & (1<<5)) == 0) {} // wait for a-n complete
        auto r = readPhy(31);
        auto duplex = (r>>4)&1, fast = (r>>3)&1;
debugf("rphy %x full-duplex %d 100-Mbit/s %d\n", r, duplex, fast);

        MAC(MCR) = (MAC(MCR) & 0xFF20810F) |
            (fast<<14) | (duplex<<11) | (1<<10); // IPCO
        msWait(1);

        // not set: MAC(FFR) MAC(HTHR) MAC(HTLR) MAC(FCR)

        DMA(BMR) = // AAB USP RDP FB PM PBL EDFE DA
            (1<<25)|(1<<24)|(1<<23)|(32<<17)|(1<<16)|(1<<14)|(32<<8)|(1<<7)|(1<<1);
        msWait(1);

        for (int i = 0; i < NTX; ++i) {
            txDesc[i].stat = 0x00D0'0000; // TCH and checksum insertion
            txDesc[i].data = txBufs[i];
            txDesc[i].next = txDesc + (i+1) % NTX;
        }

        for (int i = 0; i < NRX; ++i) {
            rxDesc[i].stat = (1<<31) | (1<<14) | BUFSZ; // OWN RCH SIZE
            rxDesc[i].data = rxBufs[i];
            rxDesc[i].next = rxDesc + (i+1) % NRX;
        }

        DMA(TDLAR) = (uint32_t) txDesc;
        DMA(RDLAR) = (uint32_t) rxDesc;

        MAC(MCR)[3] = 1; // TE
        msWait(1);
        MAC(MCR)[2] = 1; // RE
        msWait(1);

        DMA(OMR)[20] = 1; // FTF
        msWait(1);

        DMA(OMR)[13] = 1; // ST
        DMA(OMR)[1] = 1;  // SR

debugf("eth init end\n");
    }
}

void ethTest () {
    using namespace eth;
    init();

    while (true) {
        msWait(1);
        auto t = millis();
        if (t % 1000 == 0) {
            uint32_t asr = DMA(ASR);
            debugf("asr %08x %4ds", asr, t/1000);
            for (int i = 0; i < NRX; ++i)
                debugf("  %d: %08x", i, rxDesc[i].stat);
            debugf("\n");
        }
    }
}

int main () {
    fastClock(); // OpenOCD expects a 200 MHz clock for SWO
    printf("@ %d MHz\n", systemClock() / 1'000'000); // falls back to debugf

    mcu::Pin led;
    led.define("K3:P"); // set PK3 pin low to turn off the LCD backlight
    led.define("I1:P"); // ... then set the pin to the actual LED

    Serial serial (1, "A9:PU7,B7:PU7"); // TODO use the altpins info
    serial.baud(921600, systemClock() / 2);

    Printer printer (&serial, [](void* obj, uint8_t const* ptr, int len) {
        ((Serial*) obj)->send(ptr, len);
    });

    stdOut = &printer;

    //spifTest(0);
    //spifTest(1); // wipe all
    //qspiTest();
    //lcdTest();
    ethTest();

    while (true) {
        msWait(1);
        auto t = millis();
        led = (t / 100) % 5 == 0;
        if (t % 1000 == 0)
            printf("%d\n", t);
    }
}
