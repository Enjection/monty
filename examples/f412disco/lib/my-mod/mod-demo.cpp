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

/*
  Fontname: -Misc-Fixed-Medium-R-Normal--15-140-75-75-C-90-ISO10646-1
  Copyright: Public domain font.  Share and enjoy.
  Glyphs: 95/4777
  BBX Build Mode: 0
*/
const uint8_t u8g2_font_9x15_tr [1250] = 
  "_\0\3\2\4\4\4\5\5\11\17\0\375\12\375\13\377\1\224\3<\4\305 \5\0\310\63!\10\261\14"
  "\63\16\221\0\42\10\64{\63\42S\0#\16\206\31s\242\226a\211Z\206%j\1$\24\267\371\362\302"
  "A\211\42)L\322\65\11#\251\62\210\31\0%\21\247\11sB%JJ\255q\32\265\224\22\61\1&"
  "\22\247\11s\304(\213\262(T\65)\251E\225H\13'\6\61|\63\6(\14\303\373\262\222(\211z"
  "\213\262\0)\14\303\373\62\262(\213z\211\222\10*\15w\71\363J\225\266-i\252e\0+\13w\31"
  "\363\342\332\60dq\15,\11R\334\62\206$Q\0-\7\27I\63\16\1.\7\42\14\63\206\0/\14"
  "\247\11\263\323\70-\247\345\64\6\60\15\247\11\263\266J\352k\222e\23\0\61\15\247\11\363R\61\311\242"
  "\270\267a\10\62\14\247\11s\6%U\373y\30\2\63\17\247\11\63\16qZ\335\201\70V\223A\1\64"
  "\21\247\11sS\61\311\242Z\22&\303\220\306\25\0\65\17\247\11\63\16reH\304\270\254&\203\2\66"
  "\20\247\11\263\206(\215+C\42\252\326dP\0\67\16\247\11\63\16q\32\247q\32\247q\10\70\21\247"
  "\11\263\266J\232d\331VI\325$\313&\0\71\17\247\11s\6%uT\206$\256FC\4:\10r"
  "\14\63\206x\10;\12\242\334\62\206xH\22\5<\11\245\12\63\263\216i\7=\11G)\63\16\71\341"
  "\20>\12\245\12\63\322\216YG\0\77\16\247\11s\6%U\343\264\71\207\63\0@\22\247\11s\6%"
  "\65\15J\246D\223\42\347\300\240\0A\15\247\11\363\322$\253\244\326\341j\15B\22\247\11\63\6)L"
  "R\61\31\244\60I\215\311 \1C\15\247\11s\6%\225\373\232\14\12\0D\15\247\11\63\6)LR"
  "\77&\203\4E\15\247\11\63\16ry\220\342\346a\10F\14\247\11\63\16ry\220\342\316\0G\17\247"
  "\11s\6%\225\333\206\324\232\14\12\0H\12\247\11\63R\327\341\352\65I\12\245\12\63\6)\354\247A"
  "J\24\250\11\363\6\65\7r \7r \7r \12\263!\3K\21\247\11\63R\61\311\242\332\230\204"
  "QV\12\223\64L\12\247\11\63\342\376<\14\1M\20\247\11\63Ru[*JE\212\244H\265\6N"
  "\17\247\11\63RuT\62)\322\22q\265\6O\14\247\11s\6%\365\327dP\0P\15\247\11\63."
  "\251u\30\222\270\63\0Q\17\307\351r\6%\365K&U\6\65\7\4R\20\247\11\63.\251u\30\222"
  "(+\205I\252\6S\21\247\11s\6%\265\3;\240\3\252\232\14\12\0T\12\247\11\63\16Y\334\337"
  "\0U\13\247\11\63R\377\232\14\12\0V\21\247\11\63Rk\222EY\224U\302$L\322\14W\20\247"
  "\11\63RO\221\24I\221\24)\335\22\0X\20\247\11\63R\65\311*i\234&Y%U\3Y\15\247"
  "\11\63R\65\311*i\334\33\0Z\14\247\11\63\16q\332\347x\30\2[\12\304\373\62\6\255\177\33\2"
  "\134\21\247\11\63r \316\201\34\210s \7\342\34\10]\12\304\372\62\206\254\177\33\4^\12Gi\363"
  "\322$\253\244\1_\7\30\370\62\16\2`\7\63\213\63\262\2a\17w\11s\6\35\210\223aHEe"
  "H\2b\17\247\11\63\342\226!\21U\353\250\14\11\0c\14w\11s\6%\225[\223A\1d\15\247"
  "\11\263[\206D\134\35\225!\11e\16w\11s\6%U\207s\16\14\12\0f\16\247\11\363\266R\26"
  "\305\341 \306\215\0g\24\247\331r\206DL\302$\214\206(\7\6%U\223A\1h\14\247\11\63\342"
  "\226!\21U\257\1i\12\245\12stt\354i\20j\15\326\331\62u\312\332U\64&C\2k\17\247"
  "\11\63\342VMI\64\65\321\62%\15l\11\245\12\63\306\376\64\10m\20w\11\63\26%\212\244H\212"
  "\244H\212\324\0n\13w\11\63\222!\21U\257\1o\14w\11s\6%\365\232\14\12\0p\17\247\331"
  "\62\222!\21U\353\250\14I\134\6q\15\247\331r\206D\134\35\225!\211\33r\14w\11\63\242IK"
  "\302$n\5s\17w\11s\6%\325\201A\7\324dP\0t\14\227\11\263\342p\330\342n\331\2u"
  "\20w\11\63\302$L\302$L\302$\214\206$v\16w\11\63R\65\311\242\254\22&i\6w\16w"
  "\11\63RS$ER\244tK\0x\15w\11\63\322$\253\244\225\254\222\6y\15\246\331\62B\337\224"
  "%\25\223!\1z\12w\11\63\16i\257\303\20{\15\305\373\262\226\260\32ij\26V\7|\6\301\374"
  "\62>}\16\305\371\62\326\260\226jR\32V&\0~\12\67ys\64)\322\24\0\0\0\0\4\377\377"
  "\0";

Font const smallFont (u8g2_font_9x15_tr);

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
        for (int x = 10; x < 60; ++x)
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

    gfx.fg = gfx.Yellow;
    gfx.writes(smallFont, {80,15}, "Hello, Monty!");

while (true) {}
    return 2 * val;
}

//CG1 wrappers
static Lookup::Item const demo_map [] = {
};

//CG: module-end
