// mod-demo.cpp - a little demo module

#include <monty.h>
#include "twodee.h"

using namespace monty;
using namespace twodee;

#if NATIVE

#include <cstdio>

static void initLCD () {}

struct Tft {
    constexpr static char mode = 'H'; // this driver uses horizontal mode
    constexpr static auto depth = 3; // 8 colours

    static void init () {}

    static void pos (Point p)       { printf("\x1B[%d;%dH", p.y+1, p.x+1); }
    static void lim (Rect const& r) {}
    static void set (unsigned c)    { printf("\x1B[%dm ", 40 + c); }
    static void end ()              {}

    // not used by TwoDee
    static void cls () { pos({0,0}); printf("\x1B[40m\x1B[J"); }
    static void out () { pos({0,0}); printf("\x1B[m"); fflush(stdout); }
};

#elif HY_TINYSTM103T

#include <jee.h>
#include <jee/spi-ili9325.h>

using SPI = SpiGpio< PinB<5>, PinB<4>, PinB<3>, PinB<0>, 1 >;

static void initLCD () {
    // disable JTAG in AFIO-MAPR to release PB3, PB4, and PA15
    constexpr uint32_t afio = 0x40010000;
    MMIO32(afio + 0x04) |= 1 << 25;

    SPI::init();
}

struct Tft : ILI9325<SPI> {
    constexpr static char mode = 'V'; // this driver uses vertical mode
    constexpr static auto depth = 16; // colour depth (RGB565)

    static void pos (Point p) {
        write(0x20, p.x); write(0x50, p.x);
        write(0x21, p.y); write(0x52, p.y);
        SPI::enable(); SPI::transfer(0x70); out16(0x22); SPI::disable();
        SPI::enable(); SPI::transfer(0x72);
    }

    static void lim (Rect const&) {}
    static void set (unsigned c)  { out16(c); }
    static void end ()            { SPI::disable(); }
};

# else // F412-Disco

#include <jee.h>
#include <jee/mem-st7789.h>

PinF<5> backlight;
PinD<11> lcdReset;

static void initLCD () {
    Periph::bitSet(Periph::rcc+0x38, 0); // enable FMC [1] p.245
                    //   5432109876543210
    Port<'D'>::modeMap(0b1100011110110011, Pinmode::alt_out_100mhz, 12);
    Port<'E'>::modeMap(0b1111111110000000, Pinmode::alt_out_100mhz, 12);
    Port<'F'>::modeMap(0b0000000000000001, Pinmode::alt_out_100mhz, 12);

    constexpr uint32_t bcr1 = Periph::fsmc; // Periph::fmc !
    constexpr uint32_t btr1 = bcr1 + 0x04;
    MMIO32(bcr1) = (1<<12) | (1<<7) | (1<<4);
    MMIO32(btr1) = (1<<20) | (6<<8) | (2<<4) | (9<<0);
    MMIO32(bcr1) |= (1<<0);

    lcdReset.mode(Pinmode::out);
    lcdReset = 1; wait_ms(2);
    lcdReset = 0; wait_ms(2);
    lcdReset = 1; wait_ms(2);

    backlight.mode(Pinmode::out); // turn backlight on
    backlight = 1;
}

struct Tft : ST7789<0x60000000> {
    constexpr static char mode = 'V'; // this driver uses vertical mode
    constexpr static auto depth = 16; // colour depth (RGB565)

    static void pos (Point p) {
        auto xEnd = 240-1, yEnd = 240-1;
        cmd(0x2A); out16(p.y>>8); out16(p.y); out16(yEnd>>8); out16(yEnd);
        cmd(0x2B); out16(p.x>>8); out16(p.x); out16(xEnd>>8); out16(xEnd);
        cmd(0x2C);
    }

    static void lim (Rect const&) {}
    static void set (unsigned c)  { out16(c); }
    static void end ()            {}
};

#endif

TwoDee<Tft> gfx;

// from Oli Kraus' nice font collection, https://github.com/olikraus/u8g2/wiki
#define U8G2_FONT_SECTION(x)
#include "font.h"
Font const defaultFont (u8g2_font_lubR08_tr);

//CG: module graphics

//CG1 bind init
static auto f_init () -> Value {
    initLCD();
    gfx.init();
    return gfx.depth;
}

//CG1 bind fg rgb:i
static auto f_fg (int rgb) -> Value {
    auto o = gfx.fg;
    gfx.fg = rgb;
    return o;
}

//CG1 bind bg rgb:i
static auto f_bg (int rgb) -> Value {
    auto o = gfx.bg;
    gfx.bg = rgb;
    return o;
}

//CG1 bind clear
static auto f_clear () -> Value {
#if NATIVE
    gfx.cls();
#else
    gfx.clear();
#endif
    return {};
}

//CG1 bind pixel x:i y:i ? rgb:i
static auto f_pixel (ArgVec const& args, int x, int y, int rgb) -> Value {
    gfx.pixel(x, y, args.size() < 3 ? gfx.fg : rgb);
    return {};
}

//CG1 bind hline x:i y:i w:i ? rgb:i
static auto f_hline (ArgVec const& args, int x, int y, int w, int rgb) -> Value {
    gfx.hLine({x,y}, w, args.size() < 4 ? gfx.fg : rgb);
    return {};
}

//CG1 bind vline x:i y:i h:i ? rgb:i
static auto f_vline (ArgVec const& args, int x, int y, int h, int rgb) -> Value {
    gfx.vLine({x,y}, h, args.size() < 4 ? gfx.fg : rgb);
    return {};
}

//CG1 bind line x1:i y1:i x2:i y2:i
static auto f_line (int x1, int y1, int x2, int y2) -> Value {
    gfx.line({x1,y1}, {x2,y2});
    return {};
}

//CG1 bind dashed x1:i y1:i x2:i y2:i ? pat:i
static auto f_dashed (ArgVec const& args, int x1, int y1, int x2, int y2, int pat) -> Value {
    gfx.dashed({x1,y1}, {x2,y2}, args.size() < 5 ? 0x55555555 : pat);
    return {};
}

//CG1 bind box x1:i y1:i x2:i y2:i
static auto f_box (int x1, int y1, int x2, int y2) -> Value {
    gfx.box({x1,y1}, {x2,y2});
    return {};
}

//CG1 bind bfill x1:i y1:i x2:i y2:i
static auto f_bfill (int x1, int y1, int x2, int y2) -> Value {
    gfx.bFill({x1,y1}, {x2,y2});
    return {};
}

//CG1 bind round x1:i y1:i x2:i y2:i r:i
static auto f_round (int x1, int y1, int x2, int y2, int r) -> Value {
    gfx.round({x1,y1}, {x2,y2}, r);
    return {};
}

//CG1 bind rfill x1:i y1:i x2:i y2:i r:i
static auto f_rfill (int x1, int y1, int x2, int y2, int r) -> Value {
    gfx.rFill({x1,y1}, {x2,y2}, r);
    return {};
}

//CG1 bind circle x:i y:i r:i
static auto f_circle (int x, int y, int r) -> Value {
    gfx.circle({x,y}, r);
    return {};
}

//CG1 bind cfill x:i y:i r:i
static auto f_cfill (int x, int y, int r) -> Value {
    gfx.cFill({x,y}, r);
    return {};
}

//CG1 bind triangle x1:i y1:i x2:i y2:i x3:i y3:i
static auto f_triangle (int x1, int y1, int x2, int y2, int x3, int y3) -> Value {
    gfx.triangle({x1,y1}, {x2,y2}, {x3,y3});
    return {};
}

//CG1 bind tfill x1:i y1:i x2:i y2:i x3:i y3:i
static auto f_tfill (int x1, int y1, int x2, int y2, int x3, int y3) -> Value {
    gfx.tFill({x1,y1}, {x2,y2}, {x3,y3});
    return {};
}

//CG1 bind writes x:i y:i str:s
static auto f_writes (int x, int y, char const* str) -> Value {
    gfx.writes(defaultFont, {x,y}, str);
    return {};
}

//CG1 wrappers
static Lookup::Item const graphics_map [] = {
};

//CG: module-end
