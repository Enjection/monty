#include <monty.h>
#include "arch.h"

#include <jee.h>

#if STM32F103xB
UartBufDev< PinA<2>, PinA<3> > console;
#elif STM32L432xx
UartBufDev< PinA<2>, PinA<15> > console;
#else
UartBufDev< PinA<9>, PinA<10> > console;
#endif

PinB<3> led;

int printf(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); veprintf(console.putc, fmt, ap); va_end(ap);
    return 0;
}

extern "C" int puts (char const* s) { return printf("%s\n", s); }
extern "C" int putchar (int ch) { return printf("%c", ch); }

void systemReset () {
    // ARM Cortex specific
    MMIO32(0xE000ED0C) = (0x5FA<<16) | (1<<2); // SCB AIRCR reset
    while (true) {}
}

extern "C" void abort () {
    printf("\nabort\n");
    wait_ms(5000);
    systemReset();
    while (true) {} // comply with abort's "noreturn" attribute
}

extern "C" void __assert_func (char const* f, int l, char const* n, char const* e) {
    printf("\nassert(%s) in %s\n\t%s:%d\n", e, n, f, l);
    abort();
}

extern "C" void __assert (char const* f, int l, char const* e) {
    __assert_func(f, l, "-", e);
}

extern "C" void SystemInit () {}

using namespace monty;

void printDeviceInfo () {
    printf("build " __DATE__ ", " __TIME__ "\n");

    extern int g_pfnVectors [], _sidata [], _sdata [], _ebss [], _estack [];
    printf("flash 0x%p..0x%p, ram 0x%p..0x%p, stack top 0x%p\n",
            g_pfnVectors, _sidata, _sdata, _ebss, _estack);

#if STM32L4
    // the 0x1F... addresses are cpu-family specific
    printf("cpuid 0x%p, %d kB flash, package type %d\n",
            (void*) MMIO32(0xE000ED00),
            MMIO16(0x1FFF75E0),
            (int) MMIO32(0x1FFF7500) & 0x1F);

    printf("clock %d kHz, devid %p-%p-%p\n",
            (int) MMIO32(0xE000E014) + 1,
            (void*) MMIO32(0x1FFF7590),
            (void*) MMIO32(0x1FFF7594),
            (void*) MMIO32(0x1FFF7598));
#endif
}

void arch::init () {
    console.init();
#if STM32F103xB
    enableSysTick(); // no HSE crystal
#else
    console.baud(115200, fullSpeedClock());
#endif
    led.mode(Pinmode::out);

    printf("\r<---------------------------------------------------------->\n");
    printDeviceInfo();
}

void arch::idle () {
    asm ("wfi");
}

int arch::done () {
    printf("\r</>\n");
    while (true) {}
}