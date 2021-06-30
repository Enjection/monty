#include "bios.h"
#include <jee.h>

UartDev< PinA<9>, PinB<7> > console;
PinK<3> backlight;
PinI<1> led1;

Bios const bios;

void Bios::led (int on) const {
    led1 = on;
}

void Bios::delay (unsigned ms) const {
    wait_ms(ms);
}

int Bios::printf (char const* fmt, ...) const {
    va_list ap; va_start(ap, fmt); veprintf(console.putc, fmt, ap); va_end(ap);
    return 0;
}

unsigned Bios::now () const {
    return ticks;
}

int main() {
    backlight.mode(Pinmode::out); // turn backlight off
    led1.mode(Pinmode::out);

    console.init();
    console.baud(115200, fullSpeedClock()/2);
    wait_ms(200);
    bios.printf("clock %d kHz\n", (int) MMIO32(0xE000E014) + 1);
    ticks = 0;

    auto& app = *(App*) 0x2000'1000;
    app.bios = &bios;
    if (app.magic == 0x12340000)
        app.init();

    while (1) {
        bios.printf("%d\n", ticks);
        bios.led(1);
        bios.delay(100);
        bios.led(0);
        bios.delay(400);
    }
}
