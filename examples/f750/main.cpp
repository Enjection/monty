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

namespace net {
    template <typename T>
    void swap (T& a, T& b) { T t = a; a = b; b = t; }

    struct MacAddr {
        uint8_t b [6];

        void dumper () const {
            for (int i = 0; i < 6; ++i)
                debugf("%c%02x", i == 0 ? ' ' : ':', b[i]);
        }
    };

    struct Net16 {
        Net16 () {}
        constexpr Net16 (uint16_t v) : b1 {(uint8_t) (v>>8), (uint8_t) v} {}
        operator uint16_t () const { return (b1[0]<<8) | b1[1]; }
    private:
        uint8_t b1 [2];
    };

    struct Net32 {
        Net32 () {}
        constexpr Net32 (uint32_t v) : b2 {(uint16_t) (v>>16), (uint16_t) v} {}
        operator uint32_t () const { return (b2[0]<<16) | b2[1]; }
    private:
        Net16 b2 [2];
    };

    struct IpAddr : Net32 {
        constexpr IpAddr (uint32_t v =0) : Net32 (v) {}
        constexpr IpAddr (uint8_t v1, uint8_t v2, uint8_t v3, uint8_t v4)
            : Net32 ((v1<<24)|(v2<<16)|(v3<<8)|v4) {}

        void dumper () const {
            auto p = (uint8_t const*) this;
            for (int i = 0; i < 4; ++i)
                debugf("%c%d", i == 0 ? ' ' : '.', p[i]);
        }
    };

    MacAddr const wildMac {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    IpAddr const wildIp {0xFFFFFFFF};

    struct Interface {
        MacAddr const _mac;
        IpAddr _ip, _gw, _dns, _net;

        Interface (MacAddr const& mac) : _mac (mac) {}

        virtual auto canSend () -> Chunk =0;
        virtual void send (uint8_t const* p, uint32_t n) =0;
    };

    struct OptionIter {
        OptionIter (uint8_t* p) : ptr (p) {}

        operator bool () const { return *ptr != 0xFF; }

        auto next () {
            auto typ = *ptr++;
            len = *ptr++;
            ptr += len;
            return typ;
        }

        void extract (void* p, uint32_t n =0) {
            ensure (n == 0 || n == len);
            memcpy(p, ptr-len, len);
        }

        void append (uint8_t typ, void const* p, uint8_t n) {
            *ptr++ = typ;
            *ptr++ = n;
            memcpy(ptr, p, n);
            ptr += n;
            *ptr = 0xFF;
        }

        uint8_t* ptr;
        uint8_t len =0;
    };

    struct Frame {
        MacAddr _dst, _src;
        Net16 _typ;

        Frame (Net16 typ) : _typ (typ) {}

        void isReply (Interface const& ni) {
            _dst = _src;
            _src = ni._mac; 
        }

        void received (Interface&); // dispatcher
    };
    static_assert(sizeof (Frame) == 14);

    struct Arp : Frame {
        Net16 _hw =1, _proto =0x0800;
        uint8_t _macLen =6, _ipLen =4;
        Net16 _op =1;
        MacAddr _sendMac;
        IpAddr _sendIp;
        MacAddr _targMac;
        IpAddr _targIp;

        Arp () : Frame (0x0806) {}

        void isReply (Interface const& ni) {
            _dst = _src;
            _src = ni._mac; 
            _targMac = _sendMac;
            _targIp = _sendIp;
            _sendMac = _src;
            _sendIp = ni._ip;
        }

        void received (Interface& ni) {
            if (_op == 1 && _targIp == ni._ip) { // ARP request
debugf("ARP"); _sendIp.dumper(); debugf("\n");
                auto [ptr, len] = ni.canSend();
                auto& out = *(Arp*) ptr;
                ensure(len >= sizeof out);
                out = *this; // start with all fields the same
                out.isReply(ni);
                out._op = 2; // ARP reply
                ni.send(ptr, sizeof out);
            }
        }
    };
    static_assert(sizeof (Arp) == 42);

    struct Ip4 : Frame {
        uint8_t _versLen =0x45, _tos =0;
        Net16 _total, _id, _frag =0;
        uint8_t _ttl =255, _proto;
        Net16 _hcheck =0;
        IpAddr _srcIp, _dstIp;

        Ip4 (uint8_t proto) : Frame (0x0800), _proto (proto) {
            static uint16_t gid;
            _id = ++gid;
        }

        void isReply (Interface const& ni) {
            Frame::isReply(ni);
            _dstIp = _srcIp;
            _srcIp = ni._ip;
        }

        void received (Interface&); // dispatcher

        void sendIt (Interface& ni, uint16_t len) {
            _total = len - 20;
            ni.send((uint8_t const*) this, len);
        }
    };
    static_assert(sizeof (Ip4) == 34);

    struct Udp : Ip4 {
        Net16 _sPort, _dPort, _len, _sum;

        Udp () : Ip4 (17) {}

        void isReply (Interface const& ni) {
            Ip4::isReply(ni);
            swap(_sPort, _dPort);
        }

        void received (Interface&); // dispatcher

        void sendIt (Interface& ni, uint16_t len) {
            _len = len - 40;
            Ip4::sendIt(ni, len);
        }
    };
    static_assert(sizeof (Udp) == 42);

    struct Dhcp : Udp {
        uint8_t _op =1, _htype =1, _hlen =6, _hops =0;
        Net32 _tid =0;
        Net16 _sec =0, _flags =(1<<15);
        IpAddr _clientIp, _yourIp, _serverIp, _gwIp;
        uint8_t _clientHw [16], _hostName [64], _fileName [128];
        uint8_t _options [312];

        enum { Subnet=1,Router=3,Dns=6,Domain=15,Bcast=28,Ntps=42,
                ReqIp=50,Lease=51,MsgType=53,ServerIp=54,ReqList=55 };

        Dhcp () {
            _dst = wildMac;
            _dstIp = wildIp;
            _dPort = 67;
            _sPort = 68;
            memset(_clientHw, 0, 16+64+128+312);
            static uint8_t cookie [] {99, 130, 83, 99};
            memcpy(_options, cookie, sizeof cookie);
        }

        void discover (Interface& ni) {
            _src = ni._mac;
            memcpy(_clientHw, &ni._mac, sizeof ni._mac);

            OptionIter it {_options+4};
            it.append(MsgType, "\x01", 1);         // discover
            it.append(ReqList, "\x01\x03\x06", 3); // subnet router dns

            sendIt(ni, (uint32_t) it.ptr + 9 - (uint32_t) this);
        }

        auto request (Interface& ni) {
            isReply(ni);
            _op = 2; // DHCP reply
            _clientIp = _yourIp;
            memcpy(_clientHw, &ni._mac, sizeof ni._mac);

            OptionIter it {_options+4};
            it.append(MsgType, "\x03", 1); // request
            it.append(ServerIp, &_serverIp, 4);
            it.append(ReqIp, &_clientIp, 4);

            sendIt(ni, (uint32_t) it.ptr + 9 - (uint32_t) this);
        }

        void received (Interface& ni) {
            auto reply = _options[6];
            if (reply == 2 || reply == 5) { // Offer or ACK
                ni._ip = _yourIp;
                OptionIter it {_options+4};
                while (it)
                    switch (auto typ = it.next(); typ) {
                        case Subnet: it.extract(&ni._net, 4); break;
                        case Dns:    it.extract(&ni._dns, 4); break;
                        case Router: it.extract(&ni._gw,  4); break;
                        //default:     debugf("option %d\n", typ); break;
                    }
                if (reply == 2) // Offer
                    request(ni);
                else { // ACK
                    debugf("DHCP");
                    ni._ip.dumper();
                    ni._dns.dumper();
                    ni._gw.dumper();
                    ni._net.dumper();
                    debugf("\n");
                }
            }
        }
    };
    static_assert(sizeof (Dhcp) == 590);

    struct Tcp : Ip4 {
        Net16 _sPort, _dPort;
        Net32 _seq, _ack;
        Net16 _code, _window, _sum, _urg;
        uint8_t data [];

        Tcp () : Ip4 (6) {}

        void received (Interface&) {
            debugf("TCP");
            _srcIp.dumper();
            debugf(":%d ->", (int) _sPort);
            _dstIp.dumper();
            debugf(":%d win %d seq %08x ack %08x\n",
                    (int) _dPort, (int) _window, (int) _seq, (int) _ack);
        }
    };
    static_assert(sizeof (Tcp) == 54);

    void Udp::received (Interface& ni) {
        switch (_dPort) {
            case 68: ((Dhcp*) this)->received(ni); break;
            default: dumpHex((uint8_t const*) this + sizeof (Udp), _len-8);
        }
    }

    void Ip4::received (Interface& ni) {
        switch (_proto) {
            case 6:  ((Tcp*) this)->received(ni); break;
            case 17: ((Udp*) this)->received(ni); break;
            default: debugf("proto %02x\n", (int) _proto); break;
        }
    }

    void Frame::received (Interface& ni) {
        switch (_typ) {
            case 0x0800: ((Ip4*) this)->received(ni); break;
            case 0x0806: ((Arp*) this)->received(ni); break;
            //default:     debugf("frame %04x\n", (int) _typ); break;
        }
    }
}

using MacAddr = net::MacAddr;
using IpAddr = net::IpAddr;
using Interface = net::Interface;
using Frame = net::Frame;

struct Eth : Device, Interface {
    auto operator new (size_t sz) -> void* { return allocateNonCached(sz); }

    using Interface::Interface;

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
debugf("link %d ms ", millis() - t);
        writePhy(0, 0x1000); // PHY auto-negotiation
        while ((readPhy(1) & (1<<5)) == 0) {} // wait for a-n complete
        auto r = readPhy(31);
        auto duplex = (r>>4) & 1, fast = (r>>3) & 1;
debugf("full-duplex %d 100-Mbit/s %d\n", duplex, fast);

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

        DMA(OMR) = (1<<20) | (1<<13) | (1<<1); // FTF ST SR

        MAC(IMR) = (1<<9) | (1<<3); // TSTIM PMTIM
        DMA(IER)= (1<<16) | (1<<6) | (1<<0); // NISE RIE TIE

        irqInstall((uint8_t) IrqVec::ETH);
    }

    void irqHandler () override {
//debugf("E! msr %08x dsr %08x\n", (uint32_t) MAC(MSR), (uint32_t) DMA(DSR));
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

    auto canSend () -> Chunk override {
        while (!txNext->available()) {
            poll(); // keep processing incoming while waiting
            msWait(1);
        }
        return { txNext->data, BUFSZ };
    }

    void send (uint8_t const* p, uint32_t n) override {
        ensure(sizeof (Frame) <= n && n <= BUFSZ);
        if (p != txNext->data) {
            auto [ptr, len] = canSend();
            (void) len;
            memcpy(ptr, p, n);
        }
        ensure(txNext->available());
        txNext->size = n;
        txNext->stat = (0b0111<<28) | (3<<22) | (1<<20); // IC LS FS CIC TCH
        txNext = txNext->release();
        DMA(TPDR) = 0; // resume DMA
    }
};

void ethTest () {
    // TODO this doesn't seem to create an acceptable MAC address for DHCP
#if 0
    MacAddr myMac;

    constexpr auto HWID = io8<0x1FF0F420>; // F4: 0x1FF07A10
    debugf("HWID");
    for (int i = 0; i < 12; ++i) {
        uint8_t b = HWID(i);
        if (i < 6)
            myMac.b[i] = b;
        debugf(" %02x", b);
    }
    myMac.b[0] |= 0x02; // locally administered
    myMac.dumper();
    debugf("\n");

    auto eth = new Eth (myMac);
#else
    auto eth = new Eth ({0x34,0x31,0xC4,0x8E,0x32,0x66});
#endif
    debugf("eth @ %p..%p\n", eth, eth+1);

    eth->init();
    msWait(20); // TODO doesn't work without, why?

    net::Dhcp dhcp;
    dhcp.discover(*eth);

    while (true) {
        eth->poll();
        msWait(1);
    }
}

int main () {
    fastClock(); // OpenOCD expects a 200 MHz clock for SWO
    printf("@ %d MHz\n", systemClock() / 1'000'000); // falls back to debugf

    uint8_t bufs [reserveNonCached(14)]; // room for ≈2^14 bytes, i.e. 16 kB
    debugf("reserve %p..%p, %d b\n", bufs, bufs + sizeof bufs, sizeof bufs);
    ensure(bufs <= allocateNonCached(0));

    mcu::Pin led;
    led.define("K3:P"); // set PK3 pin low to turn off the LCD backlight
    led.define("I1:P"); // ... then set the pin to the actual LED

    Serial serial (1, "A9:PU7,B7:PU7"); // TODO use the altpins info
    serial.baud(921600, systemClock() / 2);

    Printer printer (&serial, [](void* obj, uint8_t const* ptr, int len) {
        ((Serial*) obj)->send(ptr, len);
    });

    stdOut = &printer;
debugf("hello %s?\n", "f750");
printf("hello %s!\n", "f750");

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
