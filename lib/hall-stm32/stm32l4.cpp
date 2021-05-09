#include "hall.h"

extern uint32_t SystemCoreClock; // CMSIS

namespace hall {
    using namespace dev;

    void enableClkWithPLL () { // using internal 16 MHz HSI
        FLASH(0x00) = 0x704;                    // flash ACR, 4 wait states
        RCC(0x00)[8] = 1;                       // HSION
        while (RCC(0x00)[10] == 0) {}           // wait for HSIRDY
        RCC(0x0C) = (1<<24) | (10<<8) | (2<<0); // 160 MHz w/ HSI
        RCC(0x00)[24] = 1;                      // PLLON
        while (RCC(0x00)[25] == 0) {}           // wait for PLLRDY
        RCC(0x08) = 0b11;                       // PLL as SYSCLK
    }

    void enableClkMaxMSI () { // using internal 48 MHz MSI
        FLASH(0x00) = 0x702;            // flash ACR, 2 wait states
        RCC(0x00)[0] = 1;               // MSION
        while (RCC(0x00)[1] == 0) {}    // wait for MSIRDY
        RCC(0x00).mask(3, 5) = 0b10111; // MSI 48 MHz
        RCC(0x08) = 0b00;               // MSI as SYSCLK
    }

    void enableClkSaver (int range) { // using MSI at 100 kHz to 4 MHz
        RCC(0x00)[0] = 1;                // MSION
        while (RCC(0x00)[1] == 0) {}     // wait for MSIRDY
        RCC(0x08) = 0b00;                // MSI as SYSCLK
        RCC(0x00) = (range<<4) | (1<<3); // MSI 48 MHz, ~HSION, ~PLLON
        FLASH(0x00) = 0;                 // no ACR, no wait states
    }

    auto fastClock (bool pll) -> uint32_t {
        PWR(0x00).mask(9, 2) = 0b01;    // VOS range 1
        while (PWR(0x14)[10] != 0) {}   // wait for ~VOSF
        if (pll) enableClkWithPLL(); else enableClkMaxMSI();
        return SystemCoreClock = pll ? 80000000 : 48000000;
    }
    auto slowClock (bool low) -> uint32_t {
        enableClkSaver(low ? 0b0000 : 0b0110);
        PWR(0x00).mask(9, 2) = 0b10;    // VOS range 2
        return SystemCoreClock = low ? 100000 : 4000000;
    }

    namespace rtc {
        enum { TR=0x00,DR=0x04,CR=0x08,ISR=0x0C,WPR=0x24,BKPR=0x50 };
        enum { BDCR=0x70 };

        void init () {
            RCC(0x40)[28] = 1; // enable PWREN
            PWR(0x00)[8] = 1;  // set DBP [1] p.481

            RCC(BDCR)[0] = 1;             // LSEON backup domain
            while (RCC(BDCR)[1] == 0) {}  // wait for LSERDY
            RCC(BDCR)[8] = 1;             // RTSEL = LSE
            RCC(BDCR)[15] = 1;            // RTCEN
        }

        auto get () -> DateTime {
            RTC(WPR) = 0xCA;  // disable write protection, [1] p.803
            RTC(WPR) = 0x53;

            RTC(ISR)[5] = 0;              // clear RSF
            while (RTC(ISR)[5] == 0) {}   // wait for RSF

            RTC(WPR) = 0xFF;  // re-enable write protection

            // shadow registers are now valid and won't change while being read
            uint32_t tod = RTC(TR);
            uint32_t doy = RTC(DR);

            DateTime dt;
            dt.ss = (tod & 0xF) + 10 * ((tod>>4) & 0x7);
            dt.mm = ((tod>>8) & 0xF) + 10 * ((tod>>12) & 0x7);
            dt.hh = ((tod>>16) & 0xF) + 10 * ((tod>>20) & 0x3);
            dt.dy = (doy & 0xF) + 10 * ((doy>>4) & 0x3);
            dt.mo = ((doy>>8) & 0xF) + 10 * ((doy>>12) & 0x1);
            // works until end 2063, will fail (i.e. roll over) in 2064 !
            dt.yr = ((doy>>16) & 0xF) + 10 * ((doy>>20) & 0x7);
            return dt;
        }

        void set (DateTime dt) {
            RTC(WPR) = 0xCA;  // disable write protection, [1] p.803
            RTC(WPR) = 0x53;

            RTC(ISR)[7] = 1;             // set INIT
            while (RTC(ISR)[6] == 0) {}  // wait for INITF
            RTC(TR) = (dt.ss + 6 * (dt.ss/10)) |
                ((dt.mm + 6 * (dt.mm/10)) << 8) |
                ((dt.hh + 6 * (dt.hh/10)) << 16);
            RTC(DR) = (dt.dy + 6 * (dt.dy/10)) |
                ((dt.mo + 6 * (dt.mo/10)) << 8) |
                ((dt.yr + 6 * (dt.yr/10)) << 16);
            RTC(ISR)[7] = 0;             // clear INIT

            RTC(WPR) = 0xFF;  // re-enable write protection
        }

        // access to the backup registers

        auto getData (int reg) -> uint32_t {
            return RTC(BKPR + 4*reg);  // regs 0..31
        }

        void setData (int reg, uint32_t val) {
            RTC(BKPR + 4*reg) = val;  // regs 0..31
        }
    }
}
