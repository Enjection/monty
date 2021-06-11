#include "boss.h"

namespace hall {
    using namespace boss;

    enum struct STM { F1, F3, F4, F7, G0, G4, H7, L0, L4 };
#if STM32F1
    constexpr auto FAMILY = STM::F1;
#elif STM32F3
    constexpr auto FAMILY = STM::F3;
#elif STM32F4
    constexpr auto FAMILY = STM::F4;
#elif STM32F7
    constexpr auto FAMILY = STM::F7;
#elif STM32G0
    constexpr auto FAMILY = STM::G0;
#elif STM32G4
    constexpr auto FAMILY = STM::G4;
#elif STM32L0
    constexpr auto FAMILY = STM::L0;
#elif STM32L4
    constexpr auto FAMILY = STM::L4;
#endif

    auto fastClock (bool pll =true) -> uint32_t;
    auto slowClock (bool low =true) -> uint32_t;
    auto systemHz () -> uint32_t;
    void idle ();
    [[noreturn]] void systemReset ();

    template <typename TYPE, uint32_t ADDR>
    struct IoBase {
        constexpr static auto addr = ADDR;

        auto& operator() (int off) const {
            return *(volatile TYPE*) (ADDR+off);
        }
    };

    template <uint32_t ADDR> using Io8 = IoBase<uint8_t,ADDR>;
    template <uint32_t ADDR> using Io16 = IoBase<uint16_t,ADDR>;

    template <uint32_t ADDR>
    struct Io32 : IoBase<uint32_t,ADDR> {
        using IoBase<uint32_t,ADDR>::operator();

        auto operator() (int off, int bit, uint8_t num =1) const {
            struct IOMask {
                int o; uint8_t b, w;

                operator uint32_t () const {
                    auto& a = *(volatile uint32_t*) (ADDR+o);
                    auto m = (1<<w)-1;
                    return (a >> b) & m;
                }
                auto operator= (uint32_t v) const {
                    auto& a = *(volatile uint32_t*) (ADDR+o);
                    auto m = (1<<w)-1;
                    a = (a & ~(m<<b)) | ((v & m)<<b);
                    return v & m;
                }
            };

            return IOMask {off + (bit >> 5), (uint8_t) (bit & 0x1F), num};
        }
    };

    template <uint32_t ADDR>
    struct Io32b : Io32<ADDR> {
        using Io32<ADDR>::operator();

        auto& operator() (int off, int bit) const {
            uint32_t a = ADDR + off + (bit>>5);
            return *(volatile uint32_t*)
                ((a & 0xF000'0000) + 0x0200'0000 + (a<<5) + ((bit&0x1F)<<2));
        }
    };

    constexpr Io32<0> anyIo32;

    namespace dev {
        //CG< devs
        constexpr Io32  <0x50040000> ADC1;
        constexpr Io32  <0x50040100> ADC2;
        constexpr Io32  <0x50040300> ADC_Common;
        constexpr Io32  <0x50060000> AES;
        constexpr Io32b <0x40006400> CAN1;
        constexpr Io32b <0x40010200> COMP;
        constexpr Io32b <0x40023000> CRC;
        constexpr Io32b <0x40006000> CRS;
        constexpr Io32b <0x40007400> DAC1;
        constexpr Io32  <0xE0042000> DBGMCU;
        constexpr Io32b <0x40016000> DFSDM;
        constexpr Io32b <0x40020000> DMA1;
        constexpr Io32b <0x40020400> DMA2;
        constexpr Io32b <0x40010400> EXTI;
        constexpr Io32b <0x40011C00> FIREWALL;
        constexpr Io32b <0x40022000> FLASH;
        constexpr Io32  <0xE000EF34> FPU;
        constexpr Io32  <0xE000ED88> FPU_CPACR;
        constexpr Io32b <0x48000000> GPIOA;
        constexpr Io32b <0x48000400> GPIOB;
        constexpr Io32b <0x48000800> GPIOC;
        constexpr Io32b <0x48000C00> GPIOD;
        constexpr Io32b <0x48001000> GPIOE;
        constexpr Io32b <0x48001C00> GPIOH;
        constexpr Io32b <0x40005400> I2C1;
        constexpr Io32b <0x40005800> I2C2;
        constexpr Io32b <0x40005C00> I2C3;
        constexpr Io32b <0x40008400> I2C4;
        constexpr Io32b <0x40003000> IWDG;
        constexpr Io32b <0x40002400> LCD;
        constexpr Io32b <0x40007C00> LPTIM1;
        constexpr Io32b <0x40009400> LPTIM2;
        constexpr Io32b <0x40008000> LPUART1;
        constexpr Io32  <0xE000ED90> MPU;
        constexpr Io32  <0xE000E100> NVIC;
        constexpr Io32  <0xE000EF00> NVIC_STIR;
        constexpr Io32b <0x40007800> OPAMP;
        constexpr Io32b <0x40007000> PWR;
        constexpr Io32  <0xA0001000> QUADSPI;
        constexpr Io32b <0x40021000> RCC;
        constexpr Io32  <0x50060800> RNG;
        constexpr Io32b <0x40002800> RTC;
        constexpr Io32b <0x40015400> SAI1;
        constexpr Io32  <0xE000ED00> SCB;
        constexpr Io32  <0xE000E008> SCB_ACTRL;
        constexpr Io32b <0x40012800> SDMMC;
        constexpr Io32b <0x40013000> SPI1;
        constexpr Io32b <0x40003800> SPI2;
        constexpr Io32b <0x40003C00> SPI3;
        constexpr Io32  <0xE000E010> STK;
        constexpr Io32b <0x40008800> SWPMI1;
        constexpr Io32b <0x40010000> SYSCFG;
        constexpr Io32b <0x40012C00> TIM1;
        constexpr Io32b <0x40014000> TIM15;
        constexpr Io32b <0x40014400> TIM16;
        constexpr Io32b <0x40000000> TIM2;
        constexpr Io32b <0x40000400> TIM3;
        constexpr Io32b <0x40001000> TIM6;
        constexpr Io32b <0x40001400> TIM7;
        constexpr Io32b <0x40024000> TSC;
        constexpr Io32b <0x40004C00> UART4;
        constexpr Io32b <0x40013800> USART1;
        constexpr Io32b <0x40004400> USART2;
        constexpr Io32b <0x40004800> USART3;
        constexpr Io32b <0x40006800> USB;
        constexpr Io32b <0x40010030> VREFBUF;
        constexpr Io32b <0x40002C00> WWDG;
        //CG>
    }

    namespace irq {
        //CG1 irqs-n
        constexpr auto limit = 85;

        enum : uint8_t {
            //CG< irqs-e
            ADC1_2         = 18,
            AES            = 79,
            CAN1_RX0       = 20,
            CAN1_RX1       = 21,
            CAN1_SCE       = 22,
            CAN1_TX        = 19,
            COMP           = 64,
            CRS            = 82,
            DFSDM1         = 61,
            DFSDM1_FLT2    = 63,
            DFSDM1_FLT3    = 42,
            DFSDM2         = 62,
            DMA1_CH1       = 11,
            DMA1_CH2       = 12,
            DMA1_CH3       = 13,
            DMA1_CH4       = 14,
            DMA1_CH5       = 15,
            DMA1_CH6       = 16,
            DMA1_CH7       = 17,
            DMA2_CH1       = 56,
            DMA2_CH2       = 57,
            DMA2_CH3       = 58,
            DMA2_CH4       = 59,
            DMA2_CH5       = 60,
            DMA2_CH6       = 68,
            DMA2_CH7       = 69,
            EXTI0          =  6,
            EXTI1          =  7,
            EXTI15_10      = 40,
            EXTI2          =  8,
            EXTI3          =  9,
            EXTI4          = 10,
            EXTI9_5        = 23,
            FLASH          =  4,
            FPU            = 81,
            I2C1_ER        = 32,
            I2C1_EV        = 31,
            I2C2_ER        = 34,
            I2C2_EV        = 33,
            I2C3_ER        = 73,
            I2C3_EV        = 72,
            I2C4_ER        = 84,
            I2C4_EV        = 83,
            LCD            = 78,
            LPTIM1         = 65,
            LPTIM2         = 66,
            LPUART1        = 70,
            PVD_PVM        =  1,
            QUADSPI        = 71,
            RCC            =  5,
            RNG            = 80,
            RTC_ALARM      = 41,
            RTC_TAMP_STAMP =  2,
            RTC_WKUP       =  3,
            SAI1           = 74,
            SDMMC1         = 49,
            SPI1           = 35,
            SPI2           = 36,
            SPI3           = 51,
            SWPMI1         = 76,
            TIM1_BRK_TIM15 = 24,
            TIM1_CC        = 27,
            TIM1_TRG_COM   = 26,
            TIM1_UP_TIM16  = 25,
            TIM2           = 28,
            TIM3           = 29,
            TIM6_DACUNDER  = 54,
            TIM7           = 55,
            TSC            = 77,
            UART4          = 52,
            USART1         = 37,
            USART2         = 38,
            USART3         = 39,
            USB            = 67,
            WWDG           =  0,
            //CG>
        };
    }

    struct Pin {
        uint8_t port :4, pin :4;

        constexpr Pin () : port (15), pin (0) {}

        auto read () const { return (gpio32(IDR)>>pin) & 1; }
        void write (int v) const { gpio32(BSRR) = (v ? 1 : 1<<16)<<pin; }

        // shorthand
        void toggle () const { write(!read()); }
        operator int () const { return read(); }
        void operator= (int v) const { write(v); }

        // pin definition string: [A-O][<pin#>]:[AFPO][DU][LNHV][<alt#>][,]
        // return -1 on error, 0 if no mode set, or the mode (always > 0)
        auto config (char const*) -> int;

        // define multiple pins, return nullptr if ok, else ptr to error
        static auto define (char const*, Pin* =nullptr, int =0) -> char const*;
    private:
        constexpr static auto OFF = FAMILY == STM::F1 ? 0x0 : 0x8;
        enum { IDR=0x08+OFF, ODR=0x0C+OFF, BSRR=0x10+OFF };

        auto gpio32 (int off) const -> volatile uint32_t& {
            return dev::GPIOA(0x400*port+off);
        }
        auto gpio32 (int off, int bit, uint8_t num =1) const {
            return dev::GPIOA(0x400*port+off, bit, num);
        }

        auto mode (int) const -> int;
        auto mode (char const* desc) const -> int;
    };

    struct BlockIRQ {
        BlockIRQ () { asm ("cpsid i"); }
        ~BlockIRQ () { asm ("cpsie i"); }
    };

    inline void nvicEnable (uint8_t irq) {
        dev::NVIC(0x00+4*(irq>>5)) = 1 << (irq&0x1F);
    }

    inline void nvicDisable (uint8_t irq) {
        dev::NVIC(0x80+4*(irq>>5)) = 1 << (irq&0x1F);
    }

    namespace systick {
        extern void (*expirer)(uint16_t,uint16_t&);

        void init (uint8_t ms =100);
        void deinit ();
        auto millis () -> uint32_t;
        auto micros () -> uint32_t;
    }

    namespace cycles {
        constexpr Io32<0xE000'1000> DWT;

        void init ();
        void deinit ();
        inline void clear () { DWT(0x04) = 0; }
        inline auto count () -> uint32_t { return DWT(0x04); }
    }

    namespace watchdog {
        auto resetCause () -> int; // watchdog: -1, nrst: 1, power: 2, other: 0
        void init (int rate =6);   // max timeout, 0 ≈ 500 ms, 6 ≈ 32 s
        void reload (int n);       // 0..4095 x 125 µs (0) .. 8 ms (6)
        void kick ();
    }

    namespace rtc {
        struct DateTime { uint8_t yr, mo, dy, hh, mm, ss; };

        void init ();
        auto get () -> DateTime;
        void set (DateTime dt);
        auto getData (int reg) -> uint32_t;
        void setData (int reg, uint32_t val);
    }

    namespace uart {
        void init (int n, char const* desc, int baud =115200);
        void deinit (int n);
        auto getc (int n) -> int;
        void putc (int n, int c);
    }
}
