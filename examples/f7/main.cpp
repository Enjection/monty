#include <mcu.h>

#include <cstdarg>
#include <printer.h>

using namespace mcu;

mcu::Pin led;

namespace swo {
    constexpr auto SWO_FREQ  = 2250000;
    constexpr auto HCLK_FREQ = 200'000'000;

    constexpr auto ITM8 =  io8<0xE000'0000>;
    constexpr auto ITM  = io32<0xE000'0000>;
    constexpr auto CDBG = io32<0xE000'EDF0>;
    constexpr auto TPIU = io32<0xE004'0000>;

    enum { TER=0xE00, TCR=0xE80, LAR=0xFB0 };
    enum { DEMCR=0x0C };

    void init () {
        TPIU(0x2004) = 0b10'0111;   // DBGMCU_CR
        CDBG(DEMCR) = CDBG(0x0C) | (1<<24) | (1<<0); // TRCENA & VC_CORERESET

        TPIU(0x0004) = 1;           // port size = 1 bit
        TPIU(0x00F0) = 2;           // trace port protocol = Uart
        TPIU(0x0010) = HCLK_FREQ / SWO_FREQ - 1;
        TPIU(0x0304) = 0x100;       // turn off formatter (0x02 bit)

        ITM(LAR) = 0xC5ACCE55;      // Unlock Lock Access Registers
        ITM(TCR)[3] = 1;            // ITM_TCR_DWTENA_Msk
        ITM(TER)[0] = 1;            // Port 0
        ITM(TCR)[0] = 1;            // ITM_TCR_ITMENA_Msk
    }

    void putc (int c) {
        if (ITM(TCR)[0] && ITM(TER)[0]) {
            while (ITM(0)[0] == 0) {}
            ITM8(0) = c;
        }
    }
}

Printer debugf (nullptr, [](void*, uint8_t const* ptr, int len) {
    while (--len >= 0)
        swo::putc(*ptr++);
});

void mcu::failAt (void const* pc, void const* lr) {
    debugf("failAt %p %p\n", pc, lr);
    for (uint32_t i = 0; i < systemClock() >> 15; ++i) {}
    systemReset();
}

int main () {
    fastClock();
    led.define("K3:P"); // set K3 to output "0", turns off the LCD backlight

    led.define("I1:P");

    msWait(1);
    while (true) {
        //msWait(100);
        auto t = millis();
debugf("hello %d\n", t);
        led = (t / 100) % 5 == 0;
    }
}
