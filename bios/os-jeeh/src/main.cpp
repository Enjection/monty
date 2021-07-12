#define BIOS_MAIN 1
#include "bios.h"

#include <jee.h>

#if STM32F103xB
#define LED_ON 0

#elif STM32F750xx
#define UART_PINS PinA<9>,PinB<7>
#define UART_DIV 2
#define LED PinI<1>

#elif DEVEBOX_H743
#define UART_DIV 4
#define LED PinA<1>
#define LED_ON 0

#elif STM32L432xx
#define UART_PINS PinA<2>,PinA<15>
#define LED PinB<3>

#endif

#ifndef UART_PINS
#define UART_PINS PinA<9>,PinA<10>
#endif

#ifndef UART_DIV
#define UART_DIV 1
#endif

#ifndef LED
#define LED PinC<13>
#endif

#ifndef LED_ON
#define LED_ON 1
#endif

UartBufDev<UART_PINS, 100> console;
LED led;

int printf(char const* fmt ...) {
    va_list ap; va_start(ap, fmt); veprintf(console.putc, fmt, ap); va_end(ap);
    return 0;
}

int Bios::vprintf (char const* fmt, va_list ap) const {
    veprintf(console.putc, fmt, ap);
    return 0;
}

void Bios::led (int on) const {
    ::led = on ? LED_ON : 1-LED_ON;
}
unsigned Bios::now () const {
    return ticks;
}
void Bios::delay (unsigned ms) const {
    wait_ms(ms);
}
int Bios::getch () const {
    return console.readable() ? console.getc() : -1;
}

static void showDeviceInfo () {
    extern int g_pfnVectors [], _sidata [], _sdata [], _ebss [], _estack [];
    printf("flash 0x%p..0x%p, ram 0x%p..0x%p, stack top 0x%p\n",
            g_pfnVectors, _sidata, _sdata, _ebss, _estack);
    // the following addresses are cpu-family specific
    constexpr uint32_t
#if STM32F1
        cpu = 0xE000ED00, rom = 0x1FFFF7E0, pkg = 0,          uid = 0x1FFFF7E8;
#elif STM32F4
        cpu = 0xE0042000, rom = 0x1FFF7A22, pkg = 0,          uid = 0x1FFF7A10;
#elif STM32F723xx
        cpu = 0xE0042000, rom = 0x1FF07A22, pkg = 0x1FF07BF0, uid = 0x1FF07A10;
#elif STM32F7
        cpu = 0xE000ED00, rom = 0x1FF0F442, pkg = 0,          uid = 0x1FF0F420;
#elif STM32H7
        cpu = 0xE000ED00, rom = 0x1FF1E880, pkg = 0,          uid = 0x1FF1E800;
#elif STM32L0
        cpu = 0xE000ED00, rom = 0x1FF8007C, pkg = 0,          uid = 0x1FF80050;
#elif STM32L4
        cpu = 0xE000ED00, rom = 0x1FFF75E0, pkg = 0x1FFF7500, uid = 0x1FFF7590;
#else
        cpu = 0, rom = 0, pkg = 0, uid = 0;
    return;
#endif
    printf("cpuid 0x%08x, %d kB flash, %d kB ram, package %d\n",
            (int) MMIO32(cpu), MMIO16(rom), (_estack - _sdata) >> 8,
            pkg != 0 ? (int) MMIO32(pkg) & 0x1F : 0); // not always available?
    printf("clock %d kHz, devid %08x-%08x-%08x\n",
            (int) MMIO32(0xE000E014) + 1,
            (int) MMIO32(uid), (int) MMIO32(uid+4), (int) MMIO32(uid+8));
}

int main() {
    console.init();
    console.baud(115200, fullSpeedClock() / UART_DIV);

#if STM32F750xx
    PinK<3> backlight;
    backlight.mode(Pinmode::out); // turn backlight off
#endif
    led.mode(Pinmode::out);

    Bios const os;

    auto& app = *(App*) 0x2000'1000;
    app.bios = &os;
    if (app.magic != 0x12340000) {
        wait_ms(500);
        printf("\r\n");
        showDeviceInfo();
    } else {
        printf("\r\n");
        if (auto r = app.init(); r != 0)
            printf("exit code %d\n");
    }

    while (1) {
        console.putc('.');
        led = LED_ON;
        wait_ms(100);
        led = 1-LED_ON;
        wait_ms(400);
    }
}
