#include "hall.h"

extern uint32_t SystemCoreClock; // CMSIS

namespace hall {
    constexpr auto PWR   = io32<0x4000'7000>;
    constexpr auto FLASH = io32<0x4002'2000>;

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
}
