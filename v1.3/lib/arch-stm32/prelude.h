// this file was generated with "inv env nucleo-l432"
// do not edit, new versions will overwrite this file

//CG: board nucleo_l432kc -device STM32L4x2 -env nucleo-l432

// from: [config:nucleo-l432]
//CG: config console uart2 -tx A2 -rx A15
//CG: config leds B3

// from: STM32L4x2.svd

//CG< periph
constexpr auto ADC1          = 0x50040000;  // ADC
constexpr auto ADC2          = 0x50040100;  // ADC
constexpr auto ADC3          = 0x50040200;  // ADC
constexpr auto ADC123_COMMON = 0x50040300;  // ADC
constexpr auto AES           = 0x50060000;  // AES
constexpr auto CAN1          = 0x40006400;  // CAN
constexpr auto COMP          = 0x40010200;  // COMP
constexpr auto CRC           = 0x40023000;  // CRC
constexpr auto CRS           = 0x40006000;  // CRS
constexpr auto DAC1          = 0x40007400;  // DAC
constexpr auto DBGMCU        = 0xE0042000;  // DBGMCU
constexpr auto DFSDM         = 0x40016000;  // DFSDM
constexpr auto DMA1          = 0x40020000;  // DMA
constexpr auto DMA2          = 0x40020400;  // DMA
constexpr auto EXTI          = 0x40010400;  // EXTI
constexpr auto FIREWALL      = 0x40011C00;  // FIREWALL
constexpr auto FLASH         = 0x40022000;  // FLASH
constexpr auto FPU           = 0xE000EF34;  // FPU
constexpr auto FPU_CPACR     = 0xE000ED88;  // FPU
constexpr auto GPIOA         = 0x48000000;  // GPIO
constexpr auto GPIOB         = 0x48000400;  // GPIO
constexpr auto GPIOC         = 0x48000800;  // GPIO
constexpr auto GPIOD         = 0x48000C00;  // GPIO
constexpr auto GPIOE         = 0x48001000;  // GPIO
constexpr auto GPIOH         = 0x48001C00;  // GPIO
constexpr auto I2C1          = 0x40005400;  // I2C
constexpr auto I2C2          = 0x40005800;  // I2C
constexpr auto I2C3          = 0x40005C00;  // I2C
constexpr auto I2C4          = 0x40008400;  // I2C
constexpr auto IWDG          = 0x40003000;  // IWDG
constexpr auto LCD           = 0x40002400;  // LCD
constexpr auto LPTIM1        = 0x40007C00;  // LPTIM
constexpr auto LPTIM2        = 0x40009400;  // LPTIM
constexpr auto LPUART1       = 0x40008000;  // USART
constexpr auto MPU           = 0xE000ED90;  // MPU
constexpr auto NVIC          = 0xE000E100;  // NVIC
constexpr auto NVIC_STIR     = 0xE000EF00;  // NVIC
constexpr auto OPAMP         = 0x40007800;  // OPAMP
constexpr auto PWR           = 0x40007000;  // PWR
constexpr auto QUADSPI       = 0xA0001000;  // QUADSPI
constexpr auto RCC           = 0x40021000;  // RCC
constexpr auto RNG           = 0x50060800;  // RNG
constexpr auto RTC           = 0x40002800;  // RTC
constexpr auto SAI1          = 0x40015400;  // SAI
constexpr auto SCB           = 0xE000ED00;  // SCB
constexpr auto SCB_ACTRL     = 0xE000E008;  // SCB
constexpr auto SDMMC         = 0x40012800;  // SDIO
constexpr auto SPI1          = 0x40013000;  // SPI
constexpr auto SPI2          = 0x40003800;  // SPI
constexpr auto SPI3          = 0x40003C00;  // SPI
constexpr auto STK           = 0xE000E010;  // STK
constexpr auto SWPMI1        = 0x40008800;  // SWPMI
constexpr auto SYSCFG        = 0x40010000;  // SYSCFG
constexpr auto TIM1          = 0x40012C00;  // TIM
constexpr auto TIM2          = 0x40000000;  // TIM
constexpr auto TIM3          = 0x40000400;  // TIM
constexpr auto TIM6          = 0x40001000;  // TIM
constexpr auto TIM7          = 0x40001400;  // TIM
constexpr auto TIM15         = 0x40014000;  // TIM
constexpr auto TIM16         = 0x40014400;  // TIM
constexpr auto TSC           = 0x40024000;  // TSC
constexpr auto UART1         = 0x40013800;  // USART
constexpr auto UART2         = 0x40004400;  // USART
constexpr auto UART3         = 0x40004800;  // USART
constexpr auto UART4         = 0x40004C00;  // USART
constexpr auto USB_FS        = 0x40006800;  // USART
constexpr auto USB_SRAM      = 0x40006C00;  // USB
constexpr auto VREFBUF       = 0x40010030;  // VREF
constexpr auto WWDG          = 0x40002C00;  // WWDG
//CG>

// useful registers
constexpr auto RCC_AHB1ENR = RCC + 0x48;
constexpr auto RCC_APB1ENR = RCC + 0x58;

enum struct IrqVec : uint8_t {
    //CG< irqvec
    ADC1                  =  18,  // ADC
    ADC3                  =  47,  // ADC
    AES                   =  79,  // AES
    CAN1_RX0              =  20,  // CAN
    CAN1_RX1              =  21,  // CAN
    CAN1_SCE              =  22,  // CAN
    CAN1_TX               =  19,  // CAN
    COMP                  =  64,  // COMP
    CRS                   =  82,  // CRS
    DFSDM1_FLT0           =  61,  // DFSDM
    DFSDM1_FLT1           =  62,  // DFSDM
    DFSDM1_FLT2           =  63,  // DFSDM
    DFSDM1_FLT3           =  42,  // DFSDM
    DMA1_CH1              =  11,  // DMA
    DMA1_CH2              =  12,  // DMA
    DMA1_CH3              =  13,  // DMA
    DMA1_CH4              =  14,  // DMA
    DMA1_CH5              =  15,  // DMA
    DMA1_CH6              =  16,  // DMA
    DMA1_CH7              =  17,  // DMA
    DMA2_CH1              =  56,  // DMA
    DMA2_CH2              =  57,  // DMA
    DMA2_CH3              =  58,  // DMA
    DMA2_CH4              =  59,  // DMA
    DMA2_CH5              =  60,  // DMA
    DMA2_CH6              =  68,  // DMA
    DMA2_CH7              =  69,  // DMA
    EXTI0                 =   6,  // EXTI
    EXTI1                 =   7,  // EXTI
    EXTI2                 =   8,  // EXTI
    EXTI3                 =   9,  // EXTI
    EXTI4                 =  10,  // EXTI
    EXTI9_5               =  23,  // EXTI
    EXTI15_10             =  40,  // EXTI
    FLASH                 =   4,  // FLASH
    FPU                   =  81,  // FPU
    I2C1_ER               =  32,  // I2C
    I2C1_EV               =  31,  // I2C
    I2C2_ER               =  34,  // I2C
    I2C2_EV               =  33,  // I2C
    I2C3_ER               =  73,  // I2C
    I2C3_EV               =  72,  // I2C
    I2C4_ER               =  84,  // I2C
    I2C4_EV               =  83,  // I2C
    LCD                   =  78,  // LCD
    LPTIM1                =  65,  // LPTIM
    LPTIM2                =  66,  // LPTIM
    LPUART1               =  70,  // USART
    PVD_PVM               =   1,  // EXTI
    QUADSPI               =  71,  // QUADSPI
    RCC                   =   5,  // RCC
    RNG                   =  80,  // RNG
    RTC_ALARM             =  41,  // RTC
    RTC_TAMP_STAMP        =   2,  // RTC
    RTC_WKUP              =   3,  // RTC
    SAI1                  =  74,  // SAI
    SDMMC1                =  49,  // SDIO
    SPI1                  =  35,  // SPI
    SPI2                  =  36,  // SPI
    SPI3                  =  51,  // SPI
    SWPMI1                =  76,  // SWPMI
    TIM1_BRK_TIM15        =  24,  // TIM
    TIM1_CC               =  27,  // TIM
    TIM1_TRG_COM          =  26,  // TIM
    TIM1_UP_TIM16         =  25,  // TIM
    TIM2                  =  28,  // TIM
    TIM3                  =  29,  // TIM
    TIM6_DACUNDER         =  54,  // TIM
    TIM7                  =  55,  // TIM
    TSC                   =  77,  // TSC
    UART1                 =  37,  // USART
    UART2                 =  38,  // USART
    UART3                 =  39,  // USART
    UART4                 =  52,  // USART
    USB_FS                =  67,  // USART
    WWDG                  =   0,  // WWDG
    //CG>
    limit = 85
};

struct DmaInfo { uint32_t base; uint8_t streams [8]; };

struct DevInfo {
    uint8_t pos :4, num :4, ena;
    uint8_t rxDma :1, rxChan :4, rxStream :3;
    uint8_t txDma :1, txChan :4, txStream :3;
    IrqVec irq;
    uint32_t base;
};

template< uint32_t N >
constexpr auto findDev (DevInfo const (&map) [N], int num) -> DevInfo const& {
    for (auto& e : map)
        if (num == e.num)
            return e;
    return map[0]; // verify num to check that it was found
}

DmaInfo const dmaInfo [] = {
    { DMA1, { 0, 11, 12, 13, 14, 15, 16, 17 }},
    { DMA2, { 0, 56, 57, 58, 59, 60, 68, 69 }},
};

DevInfo const uartInfo [] = {
    { 0, 1, 78, 0, 2, 5, 0, 2, 4, IrqVec::UART1 , UART1 },
    { 1, 2, 17, 0, 2, 6, 0, 2, 7, IrqVec::UART2 , UART2 },
    { 2, 3, 18, 0, 2, 3, 0, 2, 2, IrqVec::UART3 , UART3 },
    { 3, 4, 19, 1, 2, 5, 1, 2, 3, IrqVec::UART4 , UART4 },
};

DevInfo const spiInfo [] = {
    { 0, 1, 76, 0, 0, 0, 0, 0, 0, IrqVec::SPI1, SPI1 },
    { 1, 2, 14, 0, 0, 0, 0, 0, 0, IrqVec::SPI2, SPI2 },
    { 2, 3, 15, 0, 0, 0, 0, 0, 0, IrqVec::SPI3, SPI3 },
};

// end of generated file
