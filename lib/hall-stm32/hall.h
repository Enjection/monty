#include <cstdint>

namespace hall {
    enum struct STM { F1, F3, F4, F7, G0, H7, L0, L4 };
#if STM32F1
    constexpr auto FAMILY = STM::F1;
#elif STM32F4
    constexpr auto FAMILY = STM::F4;
#elif STM32F7
    constexpr auto FAMILY = STM::F7;
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

    struct IOWord {
        const uint32_t addr;

        operator uint32_t () const {
            return *(volatile uint32_t*) addr;
        }
        auto operator= (uint32_t v) const -> uint32_t {
            *(volatile uint32_t*) addr = v; return v;
        }
        void operator&= (uint32_t v) const {
            *(volatile uint32_t*) addr &= v;
        }
        void operator|= (uint32_t v) const {
            *(volatile uint32_t*) addr |= v;
        }
        
        auto mask (int b, uint8_t w =1) const {
            struct IOMask {
                volatile uint32_t& a; uint8_t b, w;

                operator uint32_t () const {
                    auto m = (1<<w)-1;
                    return (a >> b) & m;
                }
                auto operator= (uint32_t v) const {
                    auto m = (1<<w)-1;
                    a = (a & ~(m<<b)) | ((v & m)<<b);
                    return v & m;
                }
            };

            return IOMask {((volatile uint32_t*) addr)[b>>5],
                            (uint8_t) (b & 0x1F), w};
        }

        auto& operator[] (int b) const {
            if constexpr (FAMILY == STM::L0 || FAMILY == STM::F7)
                return mask(b);
            // FIXME bit-band only works for specific RAM and periperhal areas!
            return *(volatile uint32_t*)
                ((addr & 0xF000'0000) + 0x0200'0000 + (addr << 5) + (b << 2));
        }
    };

    template <uint32_t ADDR>
    constexpr auto io32 (int off) { return IOWord {ADDR+off}; }

    template <uint32_t ADDR>
    constexpr auto& io16 (int off) { return *(volatile uint16_t*) (ADDR+off); }

    template <uint32_t ADDR>
    constexpr auto& io8 (int off) { return *(volatile uint8_t*) (ADDR+off); }

    namespace dev {
        //CG< devs
        constexpr auto ADC1       = io32<0x50040000>;
        constexpr auto ADC2       = io32<0x50040100>;
        constexpr auto ADC_Common = io32<0x50040300>;
        constexpr auto AES        = io32<0x50060000>;
        constexpr auto CAN1       = io32<0x40006400>;
        constexpr auto COMP       = io32<0x40010200>;
        constexpr auto CRC        = io32<0x40023000>;
        constexpr auto CRS        = io32<0x40006000>;
        constexpr auto DAC1       = io32<0x40007400>;
        constexpr auto DBGMCU     = io32<0xE0042000>;
        constexpr auto DFSDM      = io32<0x40016000>;
        constexpr auto DMA1       = io32<0x40020000>;
        constexpr auto DMA2       = io32<0x40020400>;
        constexpr auto EXTI       = io32<0x40010400>;
        constexpr auto FIREWALL   = io32<0x40011C00>;
        constexpr auto FLASH      = io32<0x40022000>;
        constexpr auto FPU        = io32<0xE000EF34>;
        constexpr auto FPU_CPACR  = io32<0xE000ED88>;
        constexpr auto GPIOA      = io32<0x48000000>;
        constexpr auto GPIOB      = io32<0x48000400>;
        constexpr auto GPIOC      = io32<0x48000800>;
        constexpr auto GPIOD      = io32<0x48000C00>;
        constexpr auto GPIOE      = io32<0x48001000>;
        constexpr auto GPIOH      = io32<0x48001C00>;
        constexpr auto I2C1       = io32<0x40005400>;
        constexpr auto I2C2       = io32<0x40005800>;
        constexpr auto I2C3       = io32<0x40005C00>;
        constexpr auto I2C4       = io32<0x40008400>;
        constexpr auto IWDG       = io32<0x40003000>;
        constexpr auto LCD        = io32<0x40002400>;
        constexpr auto LPTIM1     = io32<0x40007C00>;
        constexpr auto LPTIM2     = io32<0x40009400>;
        constexpr auto LPUART1    = io32<0x40008000>;
        constexpr auto MPU        = io32<0xE000ED90>;
        constexpr auto NVIC       = io32<0xE000E100>;
        constexpr auto NVIC_STIR  = io32<0xE000EF00>;
        constexpr auto OPAMP      = io32<0x40007800>;
        constexpr auto PWR        = io32<0x40007000>;
        constexpr auto QUADSPI    = io32<0xA0001000>;
        constexpr auto RCC        = io32<0x40021000>;
        constexpr auto RNG        = io32<0x50060800>;
        constexpr auto RTC        = io32<0x40002800>;
        constexpr auto SAI1       = io32<0x40015400>;
        constexpr auto SCB        = io32<0xE000ED00>;
        constexpr auto SCB_ACTRL  = io32<0xE000E008>;
        constexpr auto SDMMC      = io32<0x40012800>;
        constexpr auto SPI1       = io32<0x40013000>;
        constexpr auto SPI2       = io32<0x40003800>;
        constexpr auto SPI3       = io32<0x40003C00>;
        constexpr auto STK        = io32<0xE000E010>;
        constexpr auto SWPMI1     = io32<0x40008800>;
        constexpr auto SYSCFG     = io32<0x40010000>;
        constexpr auto TIM1       = io32<0x40012C00>;
        constexpr auto TIM15      = io32<0x40014000>;
        constexpr auto TIM16      = io32<0x40014400>;
        constexpr auto TIM2       = io32<0x40000000>;
        constexpr auto TIM3       = io32<0x40000400>;
        constexpr auto TIM6       = io32<0x40001000>;
        constexpr auto TIM7       = io32<0x40001400>;
        constexpr auto TSC        = io32<0x40024000>;
        constexpr auto UART4      = io32<0x40004C00>;
        constexpr auto USART1     = io32<0x40013800>;
        constexpr auto USART2     = io32<0x40004400>;
        constexpr auto USART3     = io32<0x40004800>;
        constexpr auto USB        = io32<0x40006800>;
        constexpr auto VREFBUF    = io32<0x40010030>;
        constexpr auto WWDG       = io32<0x40002C00>;
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

        auto gpio32 (int off) const -> IOWord {
            return dev::GPIOA(0x400*port+off);
        }
        auto mode (int) const -> int;
        auto mode (char const* desc) const -> int;
    };

    struct BlockIRQ {
        BlockIRQ () { asm ("cpsid i"); }
        ~BlockIRQ () { asm ("cpsie i"); }
    };

    struct Device {
        uint8_t _id;

        Device ();

        void irqInstall (uint32_t irq) const;

        virtual void interrupt () { pending |= 1<<_id; }
        virtual void process () {}
        virtual void expire (uint16_t, uint16_t&) {}

        static void nvicEnable (uint8_t irq) {
            dev::NVIC(0x00+4*(irq>>5)) = 1 << (irq&0x1F);
        }

        static void nvicDisable (uint8_t irq) {
            dev::NVIC(0x80+4*(irq>>5)) = 1 << (irq&0x1F);
        }

        static void dispatch ();
        static volatile uint32_t pending;
    };

    namespace systick {
        void init (uint8_t ms =100);
        void deinit ();
        auto millis () -> uint32_t;
        auto micros () -> uint32_t;
    }

    namespace cycles {
        constexpr auto DWT = io32<0xE000'1000>;

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
}
