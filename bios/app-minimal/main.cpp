#define BIOS_INIT 1
#include <bios.h>
using namespace bios;

#include <jee.h>
#define wait_ms delay
#include <jee/mem-st7789.h>
#undef wait_ms
#include <jee/text-font.h>

ST7789<0x64000000> lcd;
TextLcd< decltype(lcd) > text;
//Font5x7< decltype(text) > console2;
//Font5x8< decltype(text) > console2;
Font11x16< decltype(text) > console2;
//Font17x24< decltype(text) > console2;

UartBufDev< PinC<12>, PinD<2> > wifiSerial;

int lprintf(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); veprintf(console2.putc, fmt, ap); va_end(ap);
    return 0;
}

void initQspi () {
    PinB<6>::mode (Pinmode::alt_out_100mhz, 10);    // qspi sel, F723-disco
    PinB<2>::mode (Pinmode::alt_out_100mhz,  9);    // qspi clk, F723-disco
    PinC<9>::mode (Pinmode::alt_out_100mhz,  9);    // qspi io0, F723-disco
    PinC<10>::mode (Pinmode::alt_out_100mhz, 9);    // qspi io1, F723-disco
    PinE<2>::mode (Pinmode::alt_out_100mhz,  9);    // qspi io2, F723-disco
    PinD<13>::mode (Pinmode::alt_out_100mhz, 9);    // qspi io3, F723-disco

    Periph::bitSet(Periph::rcc + 0x38, 1); // enable QSPI in RCC.AHB3ENR

    // see ST's ref man: RM0431 rev 3, section 13, p.347 (QUADSPI)
    auto qspi = 0xA0001000;   // p.57
    auto cr   = qspi + 0x00;   // control
    auto dcr  = qspi + 0x04;   // device configuration
    auto ccr  = qspi + 0x14;   // communication configuration
    auto lptr = qspi + 0x30;   // low-power timeout

    auto fsize = 26; // flash has 64 MB, 2^26 bytes
    auto dummy = 5;  // number of cycles between cmd and data

    MMIO32(cr) = (1<<24) | (1<<3) | (1<<0); // PRESCALER, TCEN, EN
    MMIO32(dcr) = ((fsize-1)<<16); // FSIZE

    // mem-mapped: DDRM DHHC FMODE DMODE DCYC, ABMODE ADSIZE ADMODE IMODE INS
    MMIO32(ccr) = (1<<31) | (1<<30) | (3<<26) | (3<<24) | (dummy<<18) |
                    (3<<14) | (3<<12) | (3<<10) | (1<<8) | (0xEE<<0);

    MMIO32(lptr) = (1<<10); // raise nsel after 1k idle cycles
}

void testQspi () {
    printf("qspi:\n");
    auto qmem = (uint32_t const*) 0x90000000;
    for (int n = 0; n < 25; ++n) {
        printf("%4d: %08x %08x\n", n, qmem[n*64], qmem[n*64+4]);
        delay(10);
    }
}

void initFsmc () {
    MMIO32(Periph::rcc + 0x38) |= (1<<0);  // enable FMC [1] p.245
                    //   5432109876543210
    Port<'D'>::modeMap(0b1101111110110011, Pinmode::alt_out_100mhz, 12);
    Port<'E'>::modeMap(0b1111111110000011, Pinmode::alt_out_100mhz, 12);
    Port<'F'>::modeMap(0b1111000000111111, Pinmode::alt_out_100mhz, 12);
    Port<'G'>::modeMap(0b0000001000111111, Pinmode::alt_out_100mhz, 12);
}

void initPsram () {
    constexpr uint32_t bcr1 = Periph::fmc;
    constexpr uint32_t btr1 = bcr1 + 0x04;
    MMIO32(bcr1) = (1<<20) | (1<<19) | (1<<12) | (1<<8) | (1<<7) | (1<<4);
    MMIO32(btr1) = (1<<20) | (6<<8) | (2<<4) | (9<<0);
    MMIO32(bcr1) |= (1<<0);
}

void testPsram () {
    constexpr auto psram = 0x60000000;

    MMIO32(psram+0) = 0x11223344;
    MMIO32(psram+1024*1024-4) = 0x12345678;
    MMIO32(psram+1024*1024) = 0x55667788;

    printf("psram:\n  %08x %08x\n",
        (int) MMIO32(psram+0), (int) MMIO32(psram+1024*1024-4));
}

void initLcd () {
    constexpr uint32_t bcr2 = Periph::fmc + 0x08;
    constexpr uint32_t btr2 = bcr2 + 0x04;
    MMIO32(bcr2) = (1<<12) | (1<<7) | (1<<4);
    MMIO32(btr2) = (1<<20) | (6<<8) | (2<<4) | (9<<0);
    MMIO32(bcr2) |= (1<<0);

    PinH<7> lcdReset;
    lcdReset.mode(Pinmode::out); delay(5);
    lcdReset = 1; delay(10);
    lcdReset = 0; delay(20);
    lcdReset = 1; delay(10);

    lcd.cmd(0x04); // get LCD type ID, i.e. 0x85 for ST7789
    MMIO16(lcd.addr+2);
    printf("lcd:\n  id %02x\n", MMIO16(lcd.addr+2));
    lcd.init();
}

void initBacklight (int pct) {
    PinH<11>::mode(Pinmode::alt_out, 2); // timer 5, channel 2, alt mode 2
    Timer<5> backlight;
    backlight.init(100, 10800); // 0..99 @ 108 MHz / 10800, i.e. 100 Hz
    backlight.pwm(pct, 2); // channel 2
}

void testLcd () {
    lcd.clear();
    for (int i = 0; i < 20; ++i)
        lprintf("hello %d\n", now());
}

void initWifi () {
    wifiSerial.init();
    wifiSerial.baud(1200, 216'000'000/4);

    PinG<14> reset;
    reset.mode(Pinmode::out);
    reset = 0; delay(10); reset = 1;
}

int main () {
    extern int _etext [], _sdata [], _ebss [];
    printf("code 0x%p..0x%p, data 0x%p..0x%p, " __DATE__ " " __TIME__ "\n",
            &app, _etext, _sdata, _ebss);

    initQspi();
    //testQspi();

    initFsmc();

    initPsram();
    testPsram();

    initLcd();
    initBacklight(10);
    testLcd();

    initWifi();

    printf("loop:\n");
    while (true) {
        led(now()/500 & 1);
        if (wifiSerial.readable())
            printf("%c", wifiSerial.getc());
        if (auto ch = getch(); ch >= 0)
            wifiSerial.putc(ch);
    }
}
