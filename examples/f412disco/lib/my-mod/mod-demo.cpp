// mod-demo.cpp - a little demo module

#include <monty.h>
#include <jee.h>
#include <jee/mem-st7789.h>
#include "twodee.h"

using namespace monty;
using namespace twodee;

static void initFsmcPins () {
    MMIO32(Periph::rcc + 0x38) |= (1<<0);  // enable FMC [1] p.245
                    //   5432109876543210
    Port<'D'>::modeMap(0b1100011110110011, Pinmode::alt_out_100mhz, 12);
    Port<'E'>::modeMap(0b1111111110000000, Pinmode::alt_out_100mhz, 12);
    Port<'F'>::modeMap(0b0000000000000001, Pinmode::alt_out_100mhz, 12);
}

static void initFsmcLcd () {
    initFsmcPins();

    constexpr uint32_t bcr1 = Periph::fsmc; // Periph::fmc !
    constexpr uint32_t btr1 = bcr1 + 0x04;
    MMIO32(bcr1) = (1<<12) | (1<<7) | (1<<4);
    MMIO32(btr1) = (1<<20) | (6<<8) | (2<<4) | (9<<0);
    MMIO32(bcr1) |= (1<<0);
}

PinE<0> led;
PinF<5> backlight;
PinD<11> lcdReset;

struct Tft : ST7789<0x60000000> {
    constexpr static char mode = 'V'; // this driver uses vertical mode

    enum { Black = 0x0000, Red = 0xF800, Green = 0x07E0, Blue = 0x001F,
           Yellow = 0xFFE0, Cyan = 0x07FF, Magenta = 0xF81F, White = 0xFFFF };

    static int fg, bg;

    static void pos (Point p) {
        auto xEnd = 240-1, yEnd = 240-1;
        cmd(0x2A); out16(p.y>>8); out16(p.y); out16(yEnd>>8); out16(yEnd);
        cmd(0x2B); out16(p.x>>8); out16(p.x); out16(xEnd>>8); out16(xEnd);
        cmd(0x2C);
    }

    static void set (bool f) { out16(f ? fg : bg); }

    static void lim (Rect const& r) {} // only needed for flood mode
};

int Tft::fg = Tft::White;
int Tft::bg = Tft::Black;

TwoDee<Tft> gfx;

//CG: module demo

//CG1 bind twice
static auto f_twice (ArgVec const& args) -> Value {
    //CG: args val:i

    backlight.mode(Pinmode::out); // turn backlight off
    backlight = 1;

    initFsmcLcd();

    lcdReset.mode(Pinmode::out); wait_ms(5);
    lcdReset = 1; wait_ms(10);
    lcdReset = 0; wait_ms(20);
    lcdReset = 1; wait_ms(10);

    gfx.cmd(0x04); // get LCD type ID, i.e. 0x85 for ST7789
    MMIO16(gfx.addr+2);
    printf("id %02x\n", MMIO16(gfx.addr+2));

    gfx.init();
    gfx.clear();

    for (int y = 10; y < 20; ++y)
        for (int x = 10; x < 70; ++x)
            gfx.pixel(x, y);
    
    gfx.hLine({5,2}, 230);
    gfx.vLine({5,5}, 200);

    gfx.line({230,220}, {10,220});
    gfx.line({230,220}, {230,20});

    gfx.box({80,40}, 25, 25);
    gfx.fg = gfx.Magenta;
    gfx.box({150,40}, {120,70});

    gfx.fg = gfx.Red;
    gfx.bFill({160,40}, 25, 25);
    gfx.fg = gfx.Green;
    gfx.bFill({190,70}, {220,40});

    gfx.fg = gfx.Yellow;
    gfx.line({100,100}, {150,125});
    gfx.line({150,125}, {100,150});
    gfx.line({100,150}, {125,100});
    gfx.line({125,100}, {140,150});
    gfx.line({140,150}, {100,100});

    gfx.fg = gfx.Cyan;
    gfx.triangle({30,30}, {70,60}, {60,80});
    gfx.fg = gfx.White;
    gfx.tFill({30,80}, {70,110}, {60,130});

    gfx.fg = gfx.Red;
    gfx.dashed({200,30}, {200,80}, 0x27272727);
    gfx.bg = gfx.Red;
    gfx.dashed({210,30}, {210,80}, 0x27272727);

    gfx.fg = gfx.bg = gfx.Yellow;
    gfx.dashed({200,40}, {224,64}, 0x0F0F0F0F);

    gfx.fg = gfx.White;
    gfx.bg = gfx.Black;

    gfx.circle({50,175}, 20);
    gfx.cFill({55,180}, 10);

    gfx.round({180,130}, {210, 170}, 10);
    gfx.rFill({160,180}, {220, 200}, 5);

while (true) {}
    return 2 * val;
}

//CG1 wrappers
static Lookup::Item const demo_map [] = {
};

//CG: module-end
