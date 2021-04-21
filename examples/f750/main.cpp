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

struct SdCard : SpiGpio {
    constexpr static auto TIMEOUT = 50000; // arbitrary

    void init (char const* pins) {
        SpiGpio::init(pins);

        msWait(2);

        for (int i = 0; i < 10; ++i)
            xfer(0xFF);

        auto r = cmd(0, 0, 0x95);
        //printf("c0 %d\n", r);

        r = cmd(8, 0x1AA, 0x87);
        //printf("c8 %d %08x\n", r, get32());

        do {
            cmd(55, 0);
            r = cmd(41, 1<<30);
        } while (r == 1);
        //printf("c41-1 %d\n", r);

        do {
            cmd(55, 0);
            r = cmd(41, 0);
        } while (r == 1);
        //printf("c41-0 %d\n", r);

        do {
            r = cmd(58, 0);
        } while (r == 1);
        auto v = get32();
        //printf("c58 %d %08x\n", r, v);
        sdhc = (v & (1<<30)) != 0;

        do {
            r = cmd(16, 512);
        } while (r == 1);
        //printf("c16 %d\n", r);

        disable();
    }

    auto readBlock (uint32_t page, uint8_t* buf) const -> int {
        int last = cmd(17, sdhc ? page : page * 512);
        for (int i = 0; last != 0xFE; ++i) {
            if (++i >= TIMEOUT)
                return 0;
            last = xfer(0xFF);
        }
        for (int i = 0; i < 512; ++i)
            *buf++ = xfer(0xFF);
        xfer(0xFF);
        disable();
        return 512;
    }

    auto writeBlock (uint32_t page, uint8_t const* buf) const -> int {
        cmd(24, sdhc ? page : page * 512);
        xfer(0xFF);
        xfer(0xFE);
        for (int i = 0; i < 512; ++i)
            xfer(*buf++);
        xfer(0xFF);
        disable();
        return 512;
    }

    bool sdhc =false;
private:
    auto cmd (int req, uint32_t arg, uint8_t crc =0) const -> int {
        disable();
        enable();
        wait();

        xfer(0x40 | req);
        xfer(arg >> 24);
        xfer(arg >> 16);
        xfer(arg >> 8);
        xfer(arg);
        xfer(crc);

        for (int i = 0; i < 1000; ++i)
            if (uint8_t r = xfer(0xFF); r != 0xFF)
                return r;

        return -1;
    }

    void wait () const {
        for (int i = 0; i < TIMEOUT; ++i)
            if (xfer(0xFF) == 0xFF)
                return;
    }

    auto get32 () const -> uint32_t {
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i)
            v = (v<<8) | xfer(0xFF);
        return v;
    }
};

void sdTest () {
    SdCard sd;

    // F7508-DK:
    //  PC8  D0  MISO
    //  PC9  D1
    //  PC10 D2
    //  PC11 D3  NSEL
    //  PC12 CLK SCLK
    //  PC13 Detect
    //  PD2  CMD MOSI
    sd.init("D2,C8,C12,C11");

    // Nucleo-L432, custom:
    //  PA8  D9  CS
    //  PA11 D10 MOSI
    //  PB5  D11 CLK
    //  PB4  D12 MISO
    //sd.init("A11,B4,B5,A8");

    uint8_t buf [512];
    for (int i = 0; i < 512; ++i) {
        sd.readBlock(i, buf);
        if (buf[0] != 0) {
            printf("%d\n", i);
            dumpHex(buf, 32);
        }
    }
}

void ramInit () {
    constexpr auto RCC = io32<0x4002'3800>; // defined twice ...
    RCC(0x38)[0] = 1; // FMCEN

    mcu::Pin pins [38];
    Pin::define("C3:PUV12,"
                "D0,D1,D8,D9,D10,D14,D15,"
                "E0,E1,E7,E8,E9,E10,E11,E12,E13,E14,E15,"
                "F0,F1,F2,F3,F4,F5,F11,F12,F13,F14,F15,"
                "G0,G1,G4,G5,G8,G15,"
                "H3,H5", pins, sizeof pins);

    // SDRAM timing
    constexpr auto FMC = io32<0xA000'0000>;
    enum {CR1=0x140,CR2=0x144,TR1=0x148,TR2=0x14C,CMR=0x150,RTR=0x154,SR=0x158};
    FMC(CR1) = (0<<13)|(1<<12)|(2<<10)|(0<<9)|(2<<7)|(1<<6)|(1<<4)|(1<<2)|(0<<0);
    FMC(TR1) = (1<<24)|(1<<20)|(1<<16)|(6<<12)|(3<<8)|(6<<4)|(1<<0);

    // SDRAM commands
    auto fmcWait = []() { while (FMC(SR)[5]) {} };
    fmcWait(); FMC(CMR) = (0<<9)|(0<<5)|(1<<4)|(1<<0);      // clock enable
    msWait(2);
    fmcWait(); FMC(CMR) = (0<<9)|(0<<5)|(1<<4)|(2<<0);      // precharge
    fmcWait(); FMC(CMR) = (0<<9)|(7<<5)|(1<<4)|(3<<0);      // auto-refresh
    fmcWait(); FMC(CMR) = (0x220<<9)|(0<<5)|(1<<4)|(4<<0);  // load mode
    fmcWait(); FMC(RTR) = (0x0603<<1);
}

void ramTest () {
    ramInit();

    constexpr auto RAM = io32<0xC000'0000>;
    constexpr auto N = 21; // 8 MB = 2^23 bytes = 2^21 words

    for (int i = 0; i < N; ++i)
        RAM(1<<(i+2)) = (N << 24) + i;

    for (int i = 0; i < N; ++i)
        printf("%2d %08x %08x\n", i, 1<<(i+2), (uint32_t) RAM(1<<(i+2)));
}

// referenced in Stacklet::suspend() and Event::wait()
auto monty::nowAsTicks () -> uint32_t {
    return millis();
}

void cyclesTest () {
    auto mhz = systemClock() / 1'000'000;
    cycles::start();
    auto t = cycles::count();
    printf("%d\n", t);
    printf("%d µs\n", (cycles::count() - t) / mhz);
    printf("%d µs\n", (cycles::count() - t) / mhz);
    printf("%d µs\n", (cycles::count() - t) / mhz);
    auto t1 = cycles::count();
    auto t2 = cycles::count();
    auto t3 = cycles::count();
    auto t4 = cycles::count();
    printf("%d %d %d\n", t2-t1, t3-t2, t4-t3);
}

void watchdogTest () {
    auto r = watchdog::resetCause();
    printf("wait 5s ...\n");
    msWait(5000);
    printf("reset %d\n", r);
    watchdog::init(3);
    while (true) {
        msWait(1000);
        printf("%d ms\n", millis());
    }
}

void rtcTest () {
    rtc::init();
    auto d = rtc::get();
    printf("%02d/%02d/%02d %02d:%02d:%02d\n",
            d.yr, d.mo, d.dy, d.hh, d.mm, d.ss);
#if 0
    rtc::set({21,4,19,23,59,58});
    d = rtc::get();
    printf("%02d/%02d/%02d %02d:%02d:%02d\n",
            d.yr, d.mo, d.dy, d.hh, d.mm, d.ss);
#endif
    msWait(3000);
    d = rtc::get();
    printf("%02d/%02d/%02d %02d:%02d:%02d\n",
            d.yr, d.mo, d.dy, d.hh, d.mm, d.ss);
}

void faultTest () {
    //watchdog::init(3);

    printf("abc\n");
    msWait(5);
    (uint32_t) io32<0x7000'0000>(1);
    printf("def\n");
}

void i2cTest () {
    I2cGpio i2c;
    i2c.init("H8,H7");
    i2c.detect();
}

struct I2cDev {
    I2cGpio const& bus;
    uint8_t addr;

    struct I2cReg {
        I2cGpio const& b;
        uint8_t a, r;

        operator uint8_t () const {
            return b.readReg(a, r);
        }
        auto operator= (uint8_t v) const -> uint8_t {
            b.writeReg(a, r, v); return v;
        }
    };

    auto operator[] (uint8_t reg) {
        return I2cReg {bus, addr, reg};
    }

    auto read (uint8_t reg, uint8_t* ptr, int len) const {
        return bus.readRegs(addr, reg, ptr, len);
    }
};

void touchTest () {
    I2cGpio i2c;
    i2c.init("H8,H7");
    i2c.detect();
    
    I2cDev touch {i2c, 0x38};

    msWait(200); // needed by FT5336
    printf("id %02x\n", (uint8_t) touch[0xA8]);

    auto mhz = systemClock() / 1'000'000;
    cycles::start();
    uint8_t buf [21];

    while (true) {
        msWait(100);
        auto t = cycles::count();
        if (touch.read(0x00, buf, sizeof buf)) {
            t = cycles::count() - t;
            printf("%02x %02x %02x ", buf[0], buf[1], buf[2]);
            for (uint32_t i = 3; i < sizeof buf; i += 6) {
                auto x = 256*buf[i] + buf[i+1], y = 256*buf[i+2] + buf[i+3];
                printf("%7d %4d %3d %02x",
                        x & 0xFFF, y & 0xFFF, buf[i+4], buf[i+5]);
            }
            printf("%7d µs \r", t/mhz);
        }
    }
}

struct I2cDev16 {
    I2cGpio const& bus;
    uint8_t addr;

    struct I2cReg16 {
        I2cGpio const& b;
        uint8_t a;
        uint16_t r;

        operator uint16_t () const {
            return b.readReg16(a, r);
        }
        auto operator= (uint16_t v) const -> uint16_t {
            b.writeReg16(a, r, v); return v;
        }
    };

    auto operator[] (uint16_t reg) {
        return I2cReg16 {bus, addr, reg};
    }
};

void codecTest () {
    I2cGpio i2c;
    i2c.init("H8,H7");
    i2c.detect();

#if 0
    /* Disable SAI */
    SAIx_In_DeInit();
    SAIx_Out_DeInit();

    /* PLL clock is set depending on the AudioFreq (44.1khz vs 48khz groups) */
    BSP_AUDIO_OUT_ClockConfig(&haudio_in_sai, AudioFreq, NULL); /* Clock config is shared between AUDIO IN and OUT */

    /* SAI data transfer preparation:
    Prepare the Media to be used for the audio transfer from SAI peripheral to memory */
    haudio_in_sai.Instance = AUDIO_IN_SAIx;
    if(HAL_SAI_GetState(&haudio_in_sai) == HAL_SAI_STATE_RESET)
    {
      /* Init the SAI MSP: this __weak function can be redefined by the application*/
      BSP_AUDIO_IN_MspInit(&haudio_in_sai, NULL);
    }

    /* SAI data transfer preparation:
    Prepare the Media to be used for the audio transfer from memory to SAI peripheral */
    haudio_out_sai.Instance = AUDIO_OUT_SAIx;
    if(HAL_SAI_GetState(&haudio_out_sai) == HAL_SAI_STATE_RESET)
    {
      /* Init the SAI MSP: this __weak function can be redefined by the application*/
      BSP_AUDIO_OUT_MspInit(&haudio_out_sai, NULL);
    }

    /* Configure SAI in master mode :
     *   - SAI2_block_A in master TX mode
     *   - SAI2_block_B in slave RX mode synchronous from SAI2_block_A
     */
    if (InputDevice == INPUT_DEVICE_DIGITAL_MICROPHONE_2)
    {
      slot_active = CODEC_AUDIOFRAME_SLOT_13;
    }
    else
    {
      slot_active = CODEC_AUDIOFRAME_SLOT_02;
    }
    SAIx_In_Init(SAI_MODEMASTER_TX, slot_active, AudioFreq);
#endif
    
    I2cDev16 codec {i2c, 0x1A};

    constexpr uint8_t oVol = 63;
    constexpr uint8_t iVol = 239;

    printf("id %04x\n", (uint16_t) codec[0x0]);

    static uint16_t const cmds [] = {
        0x0,   0x0000, // reset
        0xFFFF, 50,    // needed?

        // WM8994 errata
        0x102, 0x0003,
        0x817, 0x0000,
        0x102, 0x0000,

        0x39,  0x006C, // soft start
        0x01,  0x0013, // bias generator
        0xFFFF, 50,

        // headphone, enable DAC2 L&R
        0x05,  0x0303,
        0x601, 0x0001,
        0x602, 0x0001,
        0x604, 0x0000,
        0x605, 0x0000,

        // microphone 2, enable AIF1ADC2 L&R, DMICDAT2 L&R, ADC L&R
        0x04,  0x0C30,
        0x450, 0x00DB,
        0x02,  0x6000,
        0x608, 0x0002,
        0x609, 0x0002,
        0x700, 0x000E,

        0x210, 0x0033, // 16 kHz
        0x300, 0x4010, // 16b AIF1 I2S
        0x302, 0x0000, // slave mode
        0x208, 0x000A, // DSP AIF1
        0x200, 0x0001, // AIF1 clock MCLK1

        0x2D,  0x0100, // DAC1 L HPOUT1LVOL
        0x2E,  0x0100, // DAC1 R HPOUT1RVOL
        0x110, 0x8100, // startup seq HP
        0xFFFF, 300,
        0x420, 0x0000, // soft un-mute AIF1 0 DAC1 L&R

        0x03,  0x0300, // SPKxVOL PGA SPKMIXR L&R
        0x22,  0x0000, // speaker L vol 0db
        0x23,  0x0000, // speaker R vol 0db
        0x36,  0x0300, // un-mute DAC2 SPKMIX L&R
        0x01,  0x3003, // bias gen SPKOUT L&R

        // headphone/speaker
        0x51,  0x0005, // class W AIF1 0

        0x01,  0x3303, // bias gen, HPOUT1 L&R
        0x60,  0x0022, // HPOUT1 L&R intermediate
        0x4C,  0x9F25, // charge pump
        0xFFFF, 15,

        0x2D,  0x0001, // DAC1 HPOUT1LVOL
        0x2E,  0x0001, // DAC1 HPOUT1RVOL
        0x03,  0x0330, // MIXOUT L&R
        0x54,  0x0033, // DC servo start-up L&R
        0xFFFF, 257,

        0x60,  0x00EE, // HPOUT1 L&R output

        // un-mutes
        0x610, 0x00C0, // DAC1 L
        0x611, 0x00C0, // DAC1 R
        0x420, 0x0010, // AIF1 0 DAC1
        0x612, 0x00C0, // DAC2 L
        0x613, 0x00C0, // DAC2 R
        0x422, 0x0010, // AIF1 1 DAC2

        0x1C,  oVol|0x140, // HP vol L
        0x1D,  oVol|0x140, // HP vol R
        0x26,  oVol|0x140, // SPKR vol L
        0x27,  oVol|0x140, // SPKR vol R

        0x01,  0x3313, // mic bias 1
        0x620, 0x0002, // ADC oversample
        0x411, 0x3800, // AIF ADC2 HPF ena voice mode 1

        0x400, iVol|0x100, // AIF1 ADC1 vol L
        0x401, iVol|0x100, // AIF1 ADC1 vol R
        0x404, iVol|0x100, // AIF1 ADC2 vol L
        0x405, iVol|0x100, // AIF1 ADC2 vol R
    };

    for (uint32_t i = 0; i < sizeof cmds / sizeof *cmds; i += 2)
        if (cmds[i] == 0xFFFF)
            msWait(cmds[i+1]);
        else
            codec[cmds[i]] = cmds[i+1];
}

void cliTest () {
    printf("cli start\n");
    while (true) {
        auto [ptr, len] = stdIn->recv();
        if (len == 0)
            break;
        for (int i = 0; i < len; ++i)
#if 1
            putchar(ptr[i]);
#else
            printf(" %02x", ptr[i]);
        printf("\n");
#endif
        stdIn->didRecv(len);
    }
    printf("cli end\n");
}

mcu::Pin led;

static void app () {
    debugf("hello %s?\n", "f750");
    printf("hello %s!\n", "f750");

    //spifTest(0);
    //spifTest(1); // wipe all
    //qspiTest();
    //lcdTest();
    //ethTest();
    //uartTest();
    //sdTest();
    //ramTest();
    //cyclesTest();
    //watchdogTest();
    //rtcTest();
    //faultTest();
    //i2cTest();
    //touchTest();
    //codecTest();
    cliTest();
}

[[noreturn]] static void main2 () {
    uint8_t pool [12*1024];
    gcSetup(pool, sizeof pool);
    debugf("gc pool %p..%p, %d b\n", pool, pool + sizeof pool, sizeof pool);

    led.define("K3:P"); // set PK3 pin low to turn off the LCD backlight
    led.define("I1:P"); // ... then set the pin to the actual LED

    auto serial = new Serial<Uart> (1, "A9:PU7,B7:PU7"); // TODO use the altpins info
    serial->baud(921600, systemClock() / 2);

    Printer printer (serial, [](void* obj, uint8_t const* ptr, int len) {
        ((Serial<Uart>*) obj)->write(ptr, len);
    });

    stdIn = serial;
    stdOut = &printer;

    app();

    while (true) {
        msWait(1);
        auto t = millis();
        led = (t / 100) % 5 == 0;
        if (t % 10000 == 0)
            printf("%d s\n", t/1000);
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
