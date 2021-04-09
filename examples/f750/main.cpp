#include <mcu.h>

#include <cstdarg>
#include <printer.h>

using namespace mcu;

mcu::Pin led;

void spifTest (bool wipe =false) {
    // QSPI flash on F7508-DK:
    //  PD11 IO0 MOSI, PD12 IO1 MISO,
    //  PE2 IO2 (high), PD13 IO3 (high),
    //  PB2 SCLK, PB6 NSEL

    mcu::Pin dummy;
    dummy.define("E2:U");  // set io2 high
    dummy.define("D13:U"); // set io3 high

    SpiFlash spif;
    spif.init("D11,D12,B2,B6");
    spif.reset();

    //Pin pins [6];
    //Pin::define("D11:PV9,D12:PV9,E2:PV9,D13:PV9,B2:PV9,B6:PV10", pins, 6);

    printf("spif %x, size %d kB\n", spif.devId(), spif.size());

    msWait(1);
    auto t = millis();
    if (wipe) {
        printf("wipe ... ");
        spif.wipe();
    } else {
        printf("erase ... ");
        spif.erase(0x1000);
    }
    printf("%d ms\n", millis() - t);

    uint8_t buf [16];

    spif.read(0x1000, buf, sizeof buf);
    for (auto e : buf) printf(" %02x", e); printf(" %s", "\n");

    spif.write(0x1000, (uint8_t const*) "abc", 3);

    spif.read(0x1000, buf, sizeof buf);
    for (auto e : buf) printf(" %02x", e); printf(" %s", "\n");

    spif.write(0x1000, (uint8_t const*) "ABCDE", 5);

    spif.read(0x1000, buf, sizeof buf);
    for (auto e : buf) printf(" %02x", e); printf(" %s", "\n");
}

int main () {
    fastClock(); // OpenOCD expects a 200 MHz clock for SWO
    printf("@ %d MHz\n", systemClock() / 1'000'000); // falls back to debugf

    led.define("K3:P"); // set PK3 pin low to turn off the LCD backlight
    led.define("I1:P"); // ... then set the pin to the actual LED

    Serial serial (1, "A9:PU7,B7:PU7"); // TODO use the altpins info
    serial.baud(921600, systemClock() / 2);

    Printer printer (&serial, [](void* obj, uint8_t const* ptr, int len) {
        ((Serial*) obj)->send(ptr, len);
    });

    stdOut = &printer;

    //spifTest(0);
    //spifTest(1);

    while (true) {
        msWait(100);
        auto t = millis();
        led = (t / 100) % 5 == 0;
        if (led)
            printf("hello %d\n", t);
    }
}
