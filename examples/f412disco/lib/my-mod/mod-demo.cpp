// mod-demo.cpp - a little demo module

#include <monty.h>
#include <jee.h>
#include "twodee.h"

using namespace monty;
using namespace twodee;

#if HY_TINYSTM103T

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

    enum { Black = 0x0000, Red = 0xF800, Green = 0x07E0, Blue = 0x001F,
           Yellow = 0xFFE0, Cyan = 0x07FF, Magenta = 0xF81F, White = 0xFFFF };

    static int fg, bg;

    static void pos (Point p) {
        write(0x20, p.x); write(0x50, p.x); write(0x21, p.y); write(0x52, p.y);
        SPI::enable(); SPI::transfer(0x70); out16(0x22); SPI::disable();
        SPI::enable(); SPI::transfer(0x72);
    }

    static void set (bool f) {
        auto rgb = f ? fg : bg;
        SPI::transfer(rgb >> 8);
        SPI::transfer(rgb);
    }

    static void end () { SPI::disable(); }

    static void lim (Rect const&) {} // only needed for flood mode
};

int Tft::fg = Tft::White;
int Tft::bg = Tft::Black;

# else // F412-Disco

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

    static void end () {}

    static void lim (Rect const&) {} // only needed for flood mode
};

int Tft::fg = Tft::White;
int Tft::bg = Tft::Black;

#endif

TwoDee<Tft> gfx;

/*
  Fontname: -FreeType-Logisoso-Medium-R-Normal--23-230-72-72-P-30-ISO10646-1
  Copyright: Created by Mathieu Gabiot with FontForge 2.0 (http://fontforge.sf.net) - Brussels - 2009
  Glyphs: 96/527
  BBX Build Mode: 0
*/
const uint8_t u8g2_font_logisoso16_tr [1567] = 
  "`\0\3\3\4\5\4\6\6\17\27\0\374\20\374\20\0\2\13\4\1\6\2 \6\0\20T\1!\10\2"
  "\25\64\361\241D\42\11\64\266U!\231\210\2#\36\10\23T'\21MD\222\221d$\221\34\16\222\211"
  "H\345p\220\250Id\22\321D\244\4$\24H\323S'\226\322,\243\42y\237\211\245\211\215*\226\1"
  "%\30\10\23T#\263H(\42U\261\252X*V\25\251H\42\22\233\4\0&\27\10\23T\67,\311"
  "$\62\211x:$Jd\22\11\311\315\64\71\31'\7\62\264\65Q\1(\20\4\27T\25\223\210TD"
  "\372&\22\215\42\0)\20\4\27T\23\222\211\324D\372\42\222\214B\0*\22xrU'\214H\42\207"
  "\20\351\20\211H\202\62\0+\14ftT%\24\35JB\21\0,\7\62\364\63A\11-\7&\264T"
  "q(.\6\42\24\64A/\24\10\23T-\235\212\245S\261T\254*\226N\305\322)\0\60\17\10\23"
  "TE\263\214\212\374\307\322\304F\2\61\13\4\27T%:PD\372\37\62\24\10\23TE\263\214\212l"
  "S\351p*\235\252N\17\7\1\63\22\10\23T\361 \235\252\256\222\307\232K\23\33\11\0\64\22\10\23"
  "T'\25k\25k\21\351t\70\10\305J\0\65\23\10\23T\361@V\66]FE\261\62\261\64\261\221"
  "\0\66\24\10\23TE\263\214\212d\261\351\62*r,Ml$\0\67\23\10\23T\361@\244M\305\252"
  "S\261tU,\235\12\1\70\25\10\23TE\263\214\212\214\245\211\311\62*\62\226&\66\22\0\71\24\10"
  "\23TE\263\214\212\34K\223\223YL,Ml$\0:\10\222T\64A\207\21;\11\242\64\64A\207"
  "Q\2<\15\225v\134\31\223,\5gCa\0=\11gT\134\361\216\360\0>\16\225v\134\21\24\316"
  "\206\241\311R\20\0\77\22\10\23TE\263\214\212b\351p\252\254;\202X\6@*>\223\203i\7\34"
  "\242\303\331\34 \31\225H&\222\22I\211\244DR\42)\221\224HJ\244\203\351\20\232C\347\320\203\370"
  "\20\2A\23\10\23T'\26\17y\224\310$\42\235L\27\31\221(B\24\10\23TQ\273\310$Bn"
  "\22\323EV\344&\271\324\0C\20\10\23TE\263\214\212d\375\261\64\261\221\0D\17\10\23Ta\272"
  "\310\212\374o\207\211\11\0E\17\10\23T\361@\326\331d\22\353|\70\10F\16\10\23T\361@\326\331"
  "d\22\353\63\0G\21\10\23TE\263\214\212d\235\34\31K\223\223\1H\15\10\23T!\344\307\303\301"
  "\310\217\2I\10\2\25\64\361\3\1J\15\10\23T\255\377\231X\232\330H\0K\32\10\23T!\253I"
  "D\23\221\212\254F\234N\211\65\211L\244\64\221Id\3L\13\10\23T!\326\377\363\341 M\23\10"
  "\23T!,\71\35~\220H(\22\212\204\310\243\0N\23\10\23T!\254\265\231L\27\212\204\42\241\234"
  "\334:\12O\17\10\23TE\263\214\212\374\307\322\304F\2P\21\10\23TQ\273\310$B\336$\227\232"
  "X\317\0Q\16\10\23TE\263\214\212\374\307\322\344dR\25\10\23TQ\273\310$B\336$\27\223\336"
  "$\62\211L\42\24S\21\10\23TE\263\214\212\344}&\226&\66\22\0T\14\10\23T\361 \23\353"
  "\377\15\0U\15\10\23T!\344\377\261\64\261\221\0V\27\10\23T!$Jd\22\231D\244\67\211L"
  "\42\253\21\251[e\0W\21\7\25\134!\343K\204\22\241D(\207_\134\6X\25\10\23T!\224\310"
  "$\42m\22Y\215\310\261\64\21\351\215(Y\27\10\23T!\224\210&\42]fD\342t,\25K\247"
  "b\351\24\0Z\25\10\23T\361 \226\212\245b\251X*\226\212\245\342\303A\0[\13\5\27Tq\250"
  "\351\177;\14\134\21\7\25T!U\226J\247\252S\325\251\352T\0]\13\5\25Tq\230\351\177;\24"
  "^\11\66\24V%\243\210\4_\7\32\360Sq\30`\10\63\370U!\21\11a\26\311\22\134\67\255)"
  "\311\344\0\231\351\42\224\10%B\321!D\21b\21\10\23T!\326l\272\214\212|,\35&&\0c"
  "\20\310\22TE\263\214\212d\35K\23\33\11\0d\17\10\23T\255\223\345\60*\362\261\64\71\31e\22"
  "\310\22TE\263\214\212\304\303\301,\36Ml$\0f\17\10\23TG\63iV\274\334\304\372\21\0g"
  "\26\10\223Se\71\214\212\304\322\304T\23\213-\207!\361\60\261\0h\16\10\23T!\326l\272\214\212"
  "\374\243\0i\15\3\23\64!\221\3$\372\227\211\0j\17F\225S\251\216$\324\377H:D(\0k"
  "\30\10\23T!\326\255&\21MD*\62\211\254m\42\232Ld\22\331\0l\13\4\23\64!\322\377\23"
  "e\0m\33\316\22\204a\42\35,#RQH\24\22\205D!QH\24\22\205D!Q\1n\14\310"
  "\22Ta\272\214\212\374\243\0o\17\310\22TE\263\214\212|,Ml$\0p\21\10\223Sa\272\214"
  "\212|,\35&&\261f\0q\17\10\223Se\71\214\212|,MNf\35r\13\306\22D\361\20!"
  "\352\217\0s\20\307\22L\65\253\310\230\211d\32MR\33\1t\16\5\23<#\323t\230\310\364\33i"
  "\0u\14\310\22T!\344\37K\223\223\1v\23\310\22T!$Jd\22\321D\244\67\211\254Fd\2"
  "w \316\22\204!\24\22g\22\331L\42\242ID$%\222RE&\321F\222\310HD\22\221D\2"
  "x\24\310\22T!\224\210&\42\31\221*\226\22i\42-B\1y\33\10\223S#\223\310$\62\221d"
  "$\221Id\22\231DH\234n\213\214\212C\0z\21\310\22T\361 \35N\245\303\251t\70=\34\4"
  "{\20&\325S)\33\11u\33\315\206C\275\16\5|\10B\331S\361\207\2}\20&\325S!\134\324"
  "\353l$\324m\64\23\2~\11)\322Tq\10\35\2\177\6\0\20T\1\0\0\0\4\377\377\0";

Font const smallFont (u8g2_font_logisoso16_tr);

//CG: module demo

//CG1 bind twice
static auto f_twice (ArgVec const& args) -> Value {
    //CG: args val:i

    initLCD();

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
    gfx.dashed({160,40}, {184,64}, 0x0F0F0F0F);

    gfx.fg = gfx.White;
    gfx.bg = gfx.Black;

    gfx.circle({50,175}, 20);
    gfx.cFill({55,180}, 10);

    gfx.round({180,130}, {210, 170}, 10);
    gfx.rFill({160,180}, {220, 200}, 5);

    gfx.fg = gfx.Yellow;
    gfx.writes(smallFont, {80,12}, "Hello, Monty!");

while (true) {}
    return 2 * val;
}

//CG1 wrappers
static Lookup::Item const demo_map [] = {
};

//CG: module-end
