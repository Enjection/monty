#include <mcu.h>

#include <cstdarg>
#include <printer.h>

using namespace mcu;

Uart serial (2);

Printer printer (&serial, [](void* obj, uint8_t const* ptr, int len) {
    ((Uart*) obj)->send(ptr, len);
});

extern "C" int printf (const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int result = printer.vprintf(fmt, ap);
    va_end(ap);

    return result;
}

void mcu::failAt (void const* pc, void const* lr) {
    printf("failAt %p %p\n", pc, lr);
    for (uint32_t i = 0; i < systemClock() >> 15; ++i) {}
    systemReset();
}

int main () {
    fastClock();

    mcu::Pin tx, rx; // TODO use the altpins info
    tx.define("A2:PU7");
    rx.define("A15:PU3");

    serial.init();

    auto hz = mcu::systemClock();
    serial.baud(921600, hz);
    printf("%d Hz\n", hz);

    msWait(1);
    while (true) {
        //msWait(100);
        printf("%d\n", millis());
        auto [ptr, len] = serial.recv();
        printf("%d %02x\n", len, *ptr);
        serial.didRecv(1);
    }
}
