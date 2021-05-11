#include "hall.h"
#include <cstring>

using namespace hall;
using namespace hall::dev;

extern uint32_t SystemCoreClock; // CMSIS

auto hall::systemHz () -> uint32_t {
    return SystemCoreClock;
}

void hall::idle () {
    asm ("wfi");
}

void hall::systemReset () {
    for (uint32_t i = 0; i < systemHz() >> 15; ++i) {}
    SCB(0x0C) = (0x5FA<<16) | (1<<2); // SCB AIRCR reset
    while (true) {}
}

auto Pin::mode (int m) const -> int {
    if constexpr (FAMILY == STM::F1) {
        RCC(0x18) |= (1<<port) | (1<<0); // enable GPIOx and AFIO clocks
        // FIXME wrong, mode bits have changed, and it changes return value
        if (m == 0b1000 || m == 0b1100) {
            gpio32(ODR)[pin] = m & 0b0100;
            m = 0b1000;
        }
        gpio32(0x00).mask(4*pin, 4) = m; // CRL/CRH
    } else {
        // enable GPIOx clock
        switch (FAMILY) {
            case STM::F1: break;
            case STM::F3: RCC(0x14)[port] = 1; break;
            case STM::F4:
            case STM::F7: RCC(0x30)[port] = 1; asm ("dsb"); break;
            case STM::G0: RCC(0x34)[port] = 1; break;
            case STM::H7: RCC(0xE0)[port] = 1; asm ("dsb"); break;
            case STM::L0: RCC(0x2C)[port] = 1; break;
            case STM::L4: RCC(0x4C)[port] = 1; break;
        }

        gpio32(0x00).mask(2*pin, 2) = m;      // MODER
        gpio32(0x04).mask(  pin, 1) = m >> 2; // TYPER
        gpio32(0x08).mask(2*pin, 2) = m >> 3; // OSPEEDR
        gpio32(0x0C).mask(2*pin, 2) = m >> 5; // PUPDR
        gpio32(0x20).mask(4*pin, 4) = m >> 8; // AFRL/AFRH
    }
    return m;
}

auto Pin::mode (char const* desc) const -> int {
    int m = 0, a = 0;
    for (auto s = desc; *s != ',' && *s != 0; ++s)
        switch (*s) {     // 1 pp ss t mm
            case 'A': m  = 0b1'00'00'0'11; break; // m=11 analog
            case 'F': m  = 0b1'00'00'0'00; break; // m=00 float
            case 'D': m |= 0b1'10'00'0'00; break; // m=00 pull-down
            case 'U': m |= 0b1'01'00'0'00; break; // m=00 pull-up

            case 'P': m  = 0b1'00'01'0'01; break; // m=01 push-pull
            case 'O': m  = 0b1'00'01'1'01; break; // m=01 open drain

            case 'L': m &= 0b1'11'00'1'11; break; // s=00 low speed
            case 'N':                      break; // s=01 normal speed
            case 'H': m ^= 0b0'00'11'0'00; break; // s=10 high speed
            case 'V': m |= 0b0'00'10'0'00; break; // s=11 very high speed

            default:  if (*s < '0' || *s > '9' || a > 1) return -1;
                        m = (m & ~0b11) | 0b10;   // m=10 alt mode
                        a = 10 * a + *s - '0';
            case ',': break; // valid as terminator
        }
    return mode(m + (a<<8));
}

auto Pin::config (char const* desc) -> int {
    if ('A' <= *desc && *desc <= 'O') {
        port = *desc++ - 'A';
        pin = 0;
        while ('0' <= *desc && *desc <= '9')
            pin = 10 * pin + *desc++ - '0';
    }
    return port == 15 ? -1 : *desc++ != ':' ? 0 : mode(desc);
}

auto Pin::define (char const* d, Pin* v, int n) -> char const* {
    Pin dummy;
    int lastMode = 0;
    while (*d != 0) {
        if (--n < 0)
            v = &dummy;
        auto m = v->config(d);
        if (m < 0)
            break;
        if (m != 0)
            lastMode = m;
        else if (lastMode != 0)
            v->mode(lastMode);
        auto p = strchr(d, ',');
        if (p == nullptr)
            return n > 0 ? d : nullptr;
        d = p+1;
        ++v;
    }
    return d;
}

uint8_t irqMap [irq::limit];
Device* devMap [20];  // must be large enough to hold all device objects
uint8_t devNext;
volatile uint32_t Device::pending;

Device::Device () {
    //assert(devNext < sizeof devMap / sizeof *devMap);
    _id = devNext;
    devMap[devNext++] = this;
}

void Device::irqInstall (uint32_t irq) const {
    //assert(irq < sizeof irqMap);
    irqMap[irq] = _id;
    nvicEnable(irq);
}

auto Device::dispatch () -> bool {
    uint32_t pend;
    {
        BlockIRQ crit;
        pend = pending;
        pending = 0;
    }
    if (pend == 0)
        return false;
    for (int i = 0; i < devNext; ++i)
        if (pend & (1<<i))
            devMap[i]->process();
    return true;
}

extern "C" void irqHandler () {
    uint8_t irq = SCB(0x04); // ICSR
    auto idx = irqMap[irq-16];
    if (devMap[idx] != nullptr)
        devMap[idx]->interrupt();
}

namespace hall::systick {
    constexpr auto SYSTICK = io32<0xE000'E010>;

    volatile uint32_t ticks;
    uint8_t rate;

    void init (uint8_t ms) {
        if (rate > 0)
            ticks = millis();
        rate = ms;
        SYSTICK(0x4) = (ms*(systemHz()/1000))/8-1; // reload value
        SYSTICK(0x8) = 0;                          // current
        SYSTICK(0x0) = 0b011;                      // control, ÷8 mode
    }

    void deinit () {
        ticks = millis();
        SYSTICK(0x0) = 0;
        rate = 0;
    }

    auto millis () -> uint32_t {
        while (true) {
            uint32_t t = ticks, n = SYSTICK(0x8);
            if (t == ticks)
                return t + rate - (n*8)/(systemHz()/1000);
        } // ticked just now, spin one more time
    }

    auto micros () -> uint16_t {
        // scaled to work with any clock rate multiple of 100 kHz
        while (true) {
            uint32_t t = ticks, n = SCB(0x8);
            if (t == ticks)
                return (t + rate)*1000 - (n*80)/(systemHz()/100'000);
        } // ticked just now, spin one more time
    }

    struct Ticker : Device {
        void process () override {
            auto now = (uint16_t) millis();
            uint16_t limit = 100;
            for (int i = 0; i < devNext; ++i)
                devMap[i]->expire(now, limit);
            init(limit);
        }
    };

    Ticker ticker;

    extern "C" void SysTick_Handler () {
        ticks += rate;
        ticker.interrupt();
    }
}

namespace hall::cycles {
    enum { CTRL=0x000,CYCCNT=0x004,LAR=0xFB0 };
    enum { DEMCR=0xDFC };

    void init () {
        DWT(LAR) = 0xC5ACCE55;
        SCB(DEMCR) = SCB(DEMCR) | (1<<24); // set TRCENA in DEMCR
        clear();
        DWT(CTRL) = DWT(CTRL) | 1;
    }

    void deinit () {
        DWT(CTRL) = DWT(CTRL) & ~1;
    }
}

namespace hall::watchdog {
    constexpr auto IWDG = io32<0x4000'3000>;
    enum { KR=0x00,PR=0x04,RLR=0x08,SR=0x0C };

    uint8_t cause; // "semi public"

    auto resetCause () -> int {
        if (cause == 0) {
            if constexpr (FAMILY == STM::F4 || FAMILY == STM::F7) {
                enum { CSR=0x74, RMVF=24 };
                cause = RCC(CSR) >> 24;
                RCC(CSR)[RMVF] = 1; // clears all reset-cause flags
            } else if constexpr (FAMILY == STM::L4) {
                enum { CSR=0x94, RMVF=23 };
                cause = RCC(CSR) >> 24;
                RCC(CSR)[RMVF] = 1; // clears all reset-cause flags
            } // TODO add more cases, use lambda to simplify
        }
        return cause & (1<<5) ? -1 :     // iwdg
               cause & (1<<3) ? 2 :      // por/bor
               cause & (1<<2) ? 1 : 0;   // nrst, or other
    }

    void init (int rate) {
        while (IWDG(SR)[0]) {}  // wait until !PVU
        IWDG(KR) = 0x5555;      // unlock PR
        IWDG(PR) = rate;        // max timeout, 0 = 400ms, 7 = 26s
        IWDG(KR) = 0xCCCC;      // start watchdog
    }

    void reload (int n) {
        while (IWDG(SR)[1]) {}  // wait until !RVU
        IWDG(KR) = 0x5555;      // unlock PR
        IWDG(RLR) = n;
        kick();
    }

    void kick () {
        IWDG(KR) = 0xAAAA;      // reset the watchdog timout
    }
}

extern "C" {
    //CG< irqs-h
    void         ADC1_2_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void            AES_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void       CAN1_RX0_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void       CAN1_RX1_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void       CAN1_SCE_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void        CAN1_TX_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void           COMP_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void            CRS_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void         DFSDM1_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void    DFSDM1_FLT2_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void    DFSDM1_FLT3_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void         DFSDM2_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void       DMA1_CH1_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void       DMA1_CH2_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void       DMA1_CH3_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void       DMA1_CH4_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void       DMA1_CH5_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void       DMA1_CH6_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void       DMA1_CH7_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void       DMA2_CH1_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void       DMA2_CH2_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void       DMA2_CH3_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void       DMA2_CH4_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void       DMA2_CH5_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void       DMA2_CH6_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void       DMA2_CH7_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void          EXTI0_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void          EXTI1_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void      EXTI15_10_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void          EXTI2_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void          EXTI3_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void          EXTI4_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void        EXTI9_5_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void          FLASH_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void            FPU_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void        I2C1_ER_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void        I2C1_EV_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void        I2C2_ER_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void        I2C2_EV_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void        I2C3_ER_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void        I2C3_EV_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void        I2C4_ER_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void        I2C4_EV_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void            LCD_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void         LPTIM1_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void         LPTIM2_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void        LPUART1_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void        PVD_PVM_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void        QUADSPI_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void            RCC_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void            RNG_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void      RTC_ALARM_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void RTC_TAMP_STAMP_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void       RTC_WKUP_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void           SAI1_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void         SDMMC1_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void           SPI1_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void           SPI2_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void           SPI3_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void         SWPMI1_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void TIM1_BRK_TIM15_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void        TIM1_CC_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void   TIM1_TRG_COM_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void  TIM1_UP_TIM16_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void           TIM2_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void           TIM3_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void  TIM6_DACUNDER_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void           TIM7_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void            TSC_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void          UART4_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void         USART1_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void         USART2_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void         USART3_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void            USB_IRQHandler () __attribute__ ((alias ("irqHandler")));
    void           WWDG_IRQHandler () __attribute__ ((alias ("irqHandler")));
    //CG>
}
