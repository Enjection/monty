#include <monty.h>
#include <arch.h>

#include <jee.h>

using namespace monty;

#define __IM volatile const
#define __OM volatile
#define __IOM volatile

namespace swo {
    // copied from CMSIS (STM32Cube F1) - core_armv8mbl.h and core_armv8mml.h

    struct ITM_Type {
        __OM  union
        {
            __OM  uint8_t    u8;  /* 0x000 ( /W)  ITM Stimulus Port 8-bit */
            __OM  uint16_t   u16; /* 0x000 ( /W)  ITM Stimulus Port 16-bit */
            __OM  uint32_t   u32; /* 0x000 ( /W)  ITM Stimulus Port 32-bit */
        }  PORT [32U];            /* 0x000 ( /W)  ITM Stimulus Port Registers */
        uint32_t _0[864U];
        __IOM uint32_t TER;       /* 0xE00 (R/W)  ITM Trace Enable Register */
        uint32_t _1[15U];
        __IOM uint32_t TPR;       /* 0xE40 (R/W)  ITM Trace Privilege Register */
        uint32_t _2[15U];
        __IOM uint32_t TCR;       /* 0xE80 (R/W)  ITM Trace Control Register */
        uint32_t _3[29U];
        __OM  uint32_t IWR;       /* 0xEF8 ( /W)  ITM Integration Write Register */
        __IM  uint32_t IRR;       /* 0xEFC (R/ )  ITM Integration Read Register */
        __IOM uint32_t IMCR;      /* 0xF00 (R/W)  ITM Integration Mode Control Register */
        uint32_t _4[43U];
        __OM  uint32_t LAR;       /* 0xFB0 ( /W)  ITM Lock Access Register */
        __IM  uint32_t LSR;       /* 0xFB4 (R/ )  ITM Lock Status Register */
        uint32_t _5[1U];
        __IM  uint32_t DEVARCH;   /* 0xFBC (R/ )  ITM Device Architecture Register */
        uint32_t _6[4U];
        __IM  uint32_t PID4;      /* 0xFD0 (R/ )  ITM Peripheral Identification Register #4 */
        __IM  uint32_t PID5;      /* 0xFD4 (R/ )  ITM Peripheral Identification Register #5 */
        __IM  uint32_t PID6;      /* 0xFD8 (R/ )  ITM Peripheral Identification Register #6 */
        __IM  uint32_t PID7;      /* 0xFDC (R/ )  ITM Peripheral Identification Register #7 */
        __IM  uint32_t PID0;      /* 0xFE0 (R/ )  ITM Peripheral Identification Register #0 */
        __IM  uint32_t PID1;      /* 0xFE4 (R/ )  ITM Peripheral Identification Register #1 */
        __IM  uint32_t PID2;      /* 0xFE8 (R/ )  ITM Peripheral Identification Register #2 */
        __IM  uint32_t PID3;      /* 0xFEC (R/ )  ITM Peripheral Identification Register #3 */
        __IM  uint32_t CID0;      /* 0xFF0 (R/ )  ITM Component  Identification Register #0 */
        __IM  uint32_t CID1;      /* 0xFF4 (R/ )  ITM Component  Identification Register #1 */
        __IM  uint32_t CID2;      /* 0xFF8 (R/ )  ITM Component  Identification Register #2 */
        __IM  uint32_t CID3;      /* 0xFFC (R/ )  ITM Component  Identification Register #3 */
    };

    struct DWT_Type {
        __IOM uint32_t CTRL;       /* 0x000 (R/W)  Control Register */
        uint32_t _0[6U];
        __IM  uint32_t PCSR;       /* 0x01C (R/ )  Program Counter Sample Register */
        __IOM uint32_t COMP0;      /* 0x020 (R/W)  Comparator Register 0 */
        uint32_t _1[1U];
        __IOM uint32_t FUNCTION0;  /* 0x028 (R/W)  Function Register 0 */
        uint32_t _2[1U];
        __IOM uint32_t COMP1;      /* 0x030 (R/W)  Comparator Register 1 */
        uint32_t _3[1U];
        __IOM uint32_t FUNCTION1;  /* 0x038 (R/W)  Function Register 1 */
        uint32_t _4[1U];
        __IOM uint32_t COMP2;      /* 0x040 (R/W)  Comparator Register 2 */
        uint32_t _5[1U];
        __IOM uint32_t FUNCTION2;  /* 0x048 (R/W)  Function Register 2 */
        uint32_t _6[1U];
        __IOM uint32_t COMP3;      /* 0x050 (R/W)  Comparator Register 3 */
        uint32_t _7[1U];
        __IOM uint32_t FUNCTION3;  /* 0x058 (R/W)  Function Register 3 */
        uint32_t _8[1U];
        __IOM uint32_t COMP4;      /* 0x060 (R/W)  Comparator Register 4 */
        uint32_t _9[1U];
        __IOM uint32_t FUNCTION4;  /* 0x068 (R/W)  Function Register 4 */
        uint32_t _10[1U];
        __IOM uint32_t COMP5;      /* 0x070 (R/W)  Comparator Register 5 */
        uint32_t _11[1U];
        __IOM uint32_t FUNCTION5;  /* 0x078 (R/W)  Function Register 5 */
        uint32_t _12[1U];
        __IOM uint32_t COMP6;      /* 0x080 (R/W)  Comparator Register 6 */
        uint32_t _13[1U];
        __IOM uint32_t FUNCTION6;  /* 0x088 (R/W)  Function Register 6 */
        uint32_t _14[1U];
        __IOM uint32_t COMP7;      /* 0x090 (R/W)  Comparator Register 7 */
        uint32_t _15[1U];
        __IOM uint32_t FUNCTION7;  /* 0x098 (R/W)  Function Register 7 */
        uint32_t _16[1U];
        __IOM uint32_t COMP8;      /* 0x0A0 (R/W)  Comparator Register 8 */
        uint32_t _17[1U];
        __IOM uint32_t FUNCTION8;  /* 0x0A8 (R/W)  Function Register 8 */
        uint32_t _18[1U];
        __IOM uint32_t COMP9;      /* 0x0B0 (R/W)  Comparator Register 9 */
        uint32_t _19[1U];
        __IOM uint32_t FUNCTION9;  /* 0x0B8 (R/W)  Function Register 9 */
        uint32_t _20[1U];
        __IOM uint32_t COMP10;     /* 0x0C0 (R/W)  Comparator Register 10 */
        uint32_t _21[1U];
        __IOM uint32_t FUNCTION10; /* 0x0C8 (R/W)  Function Register 10 */
        uint32_t _22[1U];
        __IOM uint32_t COMP11;     /* 0x0D0 (R/W)  Comparator Register 11 */
        uint32_t _23[1U];
        __IOM uint32_t FUNCTION11; /* 0x0D8 (R/W)  Function Register 11 */
        uint32_t _24[1U];
        __IOM uint32_t COMP12;     /* 0x0E0 (R/W)  Comparator Register 12 */
        uint32_t _25[1U];
        __IOM uint32_t FUNCTION12; /* 0x0E8 (R/W)  Function Register 12 */
        uint32_t _26[1U];
        __IOM uint32_t COMP13;     /* 0x0F0 (R/W)  Comparator Register 13 */
        uint32_t _27[1U];
        __IOM uint32_t FUNCTION13; /* 0x0F8 (R/W)  Function Register 13 */
        uint32_t _28[1U];
        __IOM uint32_t COMP14;     /* 0x100 (R/W)  Comparator Register 14 */
        uint32_t _29[1U];
        __IOM uint32_t FUNCTION14; /* 0x108 (R/W)  Function Register 14 */
        uint32_t _30[1U];
        __IOM uint32_t COMP15;     /* 0x110 (R/W)  Comparator Register 15 */
        uint32_t _31[1U];
        __IOM uint32_t FUNCTION15; /* 0x118 (R/W)  Function Register 15 */
    };

    struct TPI_Type {
        __IM  uint32_t SSPSR;     /* 0x000 (R/ )  Supported Parallel Port Sizes Register */
        __IOM uint32_t CSPSR;     /* 0x004 (R/W)  Current Parallel Port Sizes Register */
        uint32_t _0[2U];
        __IOM uint32_t ACPR;      /* 0x010 (R/W)  Asynchronous Clock Prescaler Register */
        uint32_t _1[55U];
        __IOM uint32_t SPPR;      /* 0x0F0 (R/W)  Selected Pin Protocol Register */
        uint32_t _2[131U];
        __IM  uint32_t FFSR;      /* 0x300 (R/ )  Formatter and Flush Status Register */
        __IOM uint32_t FFCR;      /* 0x304 (R/W)  Formatter and Flush Control Register */
        __IOM uint32_t PSCR;      /* 0x308 (R/W)  Periodic Synchronization Control Register */
        uint32_t _3[809U];
        __OM  uint32_t LAR;       /* 0xFB0 ( /W)  Software Lock Access Register */
        __IM  uint32_t LSR;       /* 0xFB4 (R/ )  Software Lock Status Register */
        uint32_t _4[4U];
        __IM  uint32_t TYPE;      /* 0xFC8 (R/ )  Device Identifier Register */
        __IM  uint32_t DEVTYPE;   /* 0xFCC (R/ )  Device Type Register */
    };

#define ITM ((ITM_Type*) 0xE0000000UL) /*!< ITM configuration struct */
#define DWT ((DWT_Type*) 0xE0001000UL) /*!< DWT configuration struct */
#define TPI ((TPI_Type*) 0xE0040000UL) /*!< TPI configuration struct */

    constexpr auto SWO_FREQ = 2250000;
    constexpr auto HCLK_FREQ = 72000000;

    auto& DBGMCU_CR = MMIO32(0xE0042004);
    auto& DEMCR     = MMIO32(0xE000EDFC);

    void init () {
        DBGMCU_CR = (1<<5) | (1<<2) | (1<<1) | (1<<0); // IOEN STDBY STOP SLEEP
        DEMCR |= (1<<24) | (1<<0); // TRCENA & VC_CORERESET

        TPI->CSPSR = 1; // port size = 1 bit
        TPI->SPPR = 2; // protocol = UART
        TPI->ACPR = (HCLK_FREQ / SWO_FREQ) - 1;
        TPI->FFCR = 0x100; // turn off formatter (0x02 bit)

        DWT->CTRL = 0x1207;

        ITM->LAR = 0xC5ACCE55; // Unlock Lock Access Registers
        ITM->TCR |= 1<<3; //ITM_TCR_DWTENA_Msk;
        ITM->TCR |= 1<<2; //ITM_TCR_SYNCENA_Msk;
        ITM->TER |= 1<<0; // Port 0
        ITM->TCR |= 1<<0; //ITM_TCR_ITMENA_Msk;
    }

    void putc (int c) {
        if ((ITM->TCR & (1<<0)) && (ITM->TER & (1<<0))) {
            while (ITM->PORT[0].u32 == 0) {}
            ITM->PORT[0].u8 = c;
        }
    }
}

int main () {
    arch::init(12*1024);
    printf("main\n");
    swo::init();
    swo::putc('m');

    auto task = arch::cliTask();
    if (task != nullptr)
        Stacklet::ready.append(task);
    else
        printf("no task\n");

    while (Stacklet::runLoop()) {
        arch::idle();
        //swo::putc('i');
    }

    //swo::putc('d');
    printf("done\n");
    arch::done();
}
