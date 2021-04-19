#include <monty.h>
#include <mcu.h>

#include <cstdarg>
#include <printer.h>

using namespace monty;
using namespace mcu;

void dumpHex (void const* p, int n =16) {
    for (int off = 0; off < n; off += 16) {
        printf(" %03x:", off);
        for (int i = 0; i < 16; ++i) {
            if (i % 4 == 0)
                printf(" ");
            if (off+i >= n)
                printf("..");
            else {
                auto b = ((uint8_t const*) p)[off+i];
                printf("%02x", b);
            }
        }
        for (int i = 0; i < 16; ++i) {
            if (i % 4 == 0)
                printf(" ");
            auto b = ((uint8_t const*) p)[off+i];
            printf("%c", off+i >= n ? ' ' : ' ' <= b && b <= '~' ? b : '.');
        }
        printf("\n");
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
#include "lcd-stm32f7.h"
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
#include "net.h"
#include "eth-stm32f7.h"
}

void ethTest () {
    // TODO this doesn't seem to create an acceptable MAC address for DHCP
#if 0
    MacAddr myMac;

    constexpr auto HWID = io8<0x1FF0F420>; // F4: 0x1FF07A10
    printf("HWID");
    for (int i = 0; i < 12; ++i) {
        uint8_t b = HWID(i);
        if (i < 6)
            myMac.b[i] = b;
        printf(" %02x", b);
    }
    myMac.b[0] |= 0x02; // locally administered
    myMac.dumper();
    printf("\n");

    auto eth = new net::Eth (myMac);
#else
    auto eth = new net::Eth ({0x34,0x31,0xC4,0x8E,0x32,0x66});
#endif
    printf("eth @ %p..%p\n", eth, eth+1);
    printf("mac %s\n", eth->_mac.asStr());

    eth->init();
    msWait(25); // TODO doesn't work without, why?

    net::Dhcp dhcp;
    dhcp.discover(*eth);

    net::listeners.add(23); // telnet
    net::listeners.add(80); // http

    while (true) {
        eth->poll();
        //msWait(1);
    }
}

void uartTest () {
    for (int i = 0; i < 100; ++i) {
        printf("<");
        printf("%d%c", i, i % 10 == 9 ? '\n' : ' ');
    }
}

// referenced in Stacklet::suspend() and Event::wait()
auto monty::nowAsTicks () -> uint32_t {
    return millis();
}

mcu::Pin led;

static void app () {
    debugf("hello %s?\n", "f750");
    printf("hello %s!\n", "f750");

    //spifTest(0);
    //spifTest(1); // wipe all
    //qspiTest();
    //lcdTest();
    ethTest();
    //uartTest();
}

[[noreturn]] static void main2 () {
    uint8_t pool [12*1024];
    gcSetup(pool, sizeof pool);
    debugf("gc pool %p..%p, %d b\n", pool, pool + sizeof pool, sizeof pool);

    led.define("K3:P"); // set PK3 pin low to turn off the LCD backlight
    led.define("I1:P"); // ... then set the pin to the actual LED

    Serial serial (1, "A9:PU7,B7:PU7"); // TODO use the altpins info
    serial.baud(921600, systemClock() / 2);

    Printer printer (&serial, [](void* obj, uint8_t const* ptr, int len) {
        ((Serial*) obj)->write(ptr, len);
    });
    stdOut = &printer;

    app();

    while (true) {
        msWait(1);
        auto t = millis();
        led = (t / 100) % 5 == 0;
        if (t % 1000 == 0)
            printf("%d\n", t);
    }
}

int main () {
    fastClock(); // OpenOCD expects a 200 MHz clock for SWO
    debugf("@ %d MHz\n", systemClock() / 1'000'000);

    uint8_t bufs [reserveNonCached(14)]; // room for ≈2^14 bytes, i.e. 16 kB
    debugf("reserve %p..%p, %d b\n", bufs, bufs + sizeof bufs, sizeof bufs);
    ensure(bufs <= allocateNonCached(0));

    // call a separate function to avoid allocating buffers in wrong order (!)
    main2();
}
