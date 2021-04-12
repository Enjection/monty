#include <mcu.h>

#include <cstdarg>
#include <printer.h>

using namespace mcu;

void dumpHex (void const* p, int n =16) {
    for (int off = 0; off < n; off += 16) {
        debugf(" %03x:", off);
        for (int i = 0; i < 16; ++i) {
            if (i % 4 == 0)
                debugf(" ");
            auto b = ((uint8_t const*) p)[off+i];
            debugf("%02x", b);
        }
        for (int i = 0; i < 16; ++i) {
            if (i % 4 == 0)
                debugf(" ");
            auto b = ((uint8_t const*) p)[off+i];
            debugf("%c", ' ' <= b && b <= '~' ? b : '.');
        }
        debugf("\n");
    }
}

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

    constexpr auto DMA = io32<ETHERNET_DMA>;
    enum { BMR=0x00,TPDR=0x04,RDLAR=0x0C,TDLAR=0x10,ASR=0x14,OMR=0x18 };

    constexpr auto MAC = io32<ETHERNET_MAC>;
    enum { CR=0x00,FFR=0x04,HTHR=0x08,HTLR=0x0C,MIIAR=0x10,MIIDR=0x14,FCR=0x18,
           A0HR=0x40,A0LR=0x44 };

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
    };

    constexpr auto NRX = 4, NTX = 4, BUFSZ = 1524;
    DmaDesc rxDesc [NRX], txDesc [NTX];
    uint8_t rxBufs [NRX][BUFSZ], txBufs [NTX][BUFSZ];

    void init (uint8_t const* mac) {
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
debugf("link %d ms\n", millis() - t);
        writePhy(0, 0x1000); // PHY auto-negotiation
        while ((readPhy(1) & (1<<5)) == 0) {} // wait for a-n complete
        auto r = readPhy(31);
        auto duplex = (r>>4) & 1, fast = (r>>3) & 1;
debugf("rphy %x full-duplex %d 100-Mbit/s %d\n", r, duplex, fast);

        MAC(CR) = (1<<15) | (fast<<14) | (duplex<<11) | (1<<10); // IPCO
        msWait(1);

        // not set: MAC(FFR) MAC(HTHR) MAC(HTLR) MAC(FCR)

        DMA(BMR) = // AAB USP RDP FB PM PBL EDFE DA
            (1<<25)|(1<<24)|(1<<23)|(32<<17)|(1<<16)|(1<<14)|(32<<8)|(1<<7)|(1<<1);
        msWait(1);

        MAC(A0HR) = *(uint16_t const*) (mac + 4);
        MAC(A0LR) = *(uint32_t const*) mac;

        for (int i = 0; i < NTX; ++i) {
            txDesc[i].stat = 0; // not owned by DMA
            txDesc[i].data = txBufs[i];
            txDesc[i].next = txDesc + (i+1) % NTX;
        }

        for (int i = 0; i < NRX; ++i) {
            rxDesc[i].stat = (1<<31); // OWN
            rxDesc[i].size = (1<<14) | BUFSZ; // RCH SIZE
            rxDesc[i].data = rxBufs[i];
            rxDesc[i].next = rxDesc + (i+1) % NRX;
        }

        DMA(TDLAR) = (uint32_t) txDesc;
        DMA(RDLAR) = (uint32_t) rxDesc;

        MAC(CR)[3] = 1; msWait(1); // TE
        MAC(CR)[2] = 1; msWait(1); // RE

        DMA(OMR)[20] = 1; // FTF
        DMA(OMR)[13] = 1; // ST
        DMA(OMR)[1] = 1;  // SR
        msWait(1);
    }

    void txWake () {
        DMA(TPDR) = 0; // resume DMA
    }

// no hardware-specific code past this point ===================================

    DmaDesc* rxNext = rxDesc;
    DmaDesc* txNext = txDesc;

    template <typename T>
    void swap (T& a, T& b) { T t = a; a = b; b = t; }

    struct MacAddr {
        uint8_t b [6];
    };

    struct Net16 {
        constexpr Net16 (uint16_t v) : b1 {(uint8_t) (v>>8), (uint8_t) v} {}
        operator uint16_t () const { return (b1[0]<<8) | b1[1]; }
    private:
        uint8_t b1 [2];
    };

    struct Net32 {
        constexpr Net32 (uint32_t v) : b2 {(uint16_t) (v>>16), (uint16_t) v} {}
        operator uint32_t () const { return (b2[0]<<16) | b2[1]; }
    private:
        Net16 b2 [2];
    };

    struct IpAddr : Net32 {
        constexpr IpAddr (uint8_t v1, uint8_t v2, uint8_t v3, uint8_t v4)
            : Net32 ((v1<<24)|(v2<<16)|(v3<<8)|v4) {}

        void dumper () const {
            auto p = (uint8_t const*) this;
            for (int i = 0; i < 4; ++i)
                debugf("%c%d", i == 0 ? ' ' : '.', p[i]);
        }
    };

    MacAddr const myMac {0x11,0x22,0x33,0x44,0x55,0x66};
    IpAddr const myIp {192,168,188,17};

    struct Frame {
        MacAddr _dst, _src;
        Net16 _typ;

        void isReply () { _dst = _src; _src = myMac; }

        void received ();
    };
    static_assert(sizeof (Frame) == 14);

    void poll () {
        while (rxNext->stat >= 0) {
            auto f = (Frame*) rxNext->data;
            f->received();
            rxNext->stat = (1<<31); // OWN
            rxNext = rxNext->next;
        }
    }

    auto canSend () -> Chunk {
        while (txNext->stat < 0) {
            poll(); // keep processing incoming while waiting
            msWait(1);
        }
        return { txNext->data, BUFSZ };
    }

    void send (uint8_t const* p, uint32_t n) {
        ensure(p == txNext->data);
        ensure(sizeof (Frame) <= n && n <= BUFSZ);
        txNext->size = n;
        txNext->stat = (0b1011<<28) | (3<<22) | (1<<20); // OWN LS FS CIC TCH
        txNext = txNext->next;
        txWake();
    }

    struct Arp : Frame {
        Net16 _hw, _proto;
        uint8_t _macLen, _ipLen;
        Net16 _op;
        MacAddr _sendMac;
        IpAddr _sendIp;
        MacAddr _targMac;
        IpAddr _targIp;

        void isReply () {
            Frame::isReply();
            _targMac = _sendMac;
            _targIp = _sendIp;
            _sendMac = myMac;
            _sendIp = myIp;
        }

        void received () {
            if (_op == 1 && _targIp == myIp) { // ARP request
debugf("ARP"); _sendIp.dumper(); debugf("\n");
                auto [ptr, len] = canSend();
                auto& out = *(Arp*) ptr;
                ensure(len >= sizeof out);
                out = *this; // start with all fields the same
                out.isReply();
                out._op = 2; // ARP reply
                send(ptr, sizeof out);
            }
        }
    };
    static_assert(sizeof (Arp) == 42);

    struct Ip4 : Frame {
        uint8_t _versLen, _service;
        Net16 _total, _id, _frag;
        uint8_t _ttl, _proto;
        Net16 _hCheck;
        IpAddr _src, _dst;

        void received ();
    };
    static_assert(sizeof (Ip4) == 34);

    struct Udp : Ip4 {
        Net16 _sPort, _dPort, _len, _sum;
        uint8_t data [];

        void received () {
            dumpHex(data, _len-8);
        }
    };
    static_assert(sizeof (Udp) == 42);

    struct Tcp : Ip4 {
        Net16 _sPort, _dPort;
        Net32 _seq, _ack;
        Net16 _code, _window, _sum, _urg;
        uint8_t data [];

        void received () {}
    };
    static_assert(sizeof (Tcp) == 54);

    void Ip4::received () {
        switch (_proto) {
            case 6:  ((Tcp*) this)->received(); break;
            case 17: ((Udp*) this)->received(); break;
        }
    }

    void Frame::received () {
        switch (_typ) {
            case 0x0806: ((Arp*) this)->received(); break;
            case 0x0800: ((Ip4*) this)->received(); break;
            //default:     debugf("frame %04x\n", (int) _typ); break;
        }
    }
}

void ethTest () {
    eth::init(eth::myMac.b);
    while (true) {
        eth::poll();
        msWait(1);
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
debugf("hello\n");
printf("hello %s\n", "f750");

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
