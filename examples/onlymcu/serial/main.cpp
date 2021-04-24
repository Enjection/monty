#include <mcu.h>

#include <cstdarg>
#include <printer.h>

using namespace mcu;

void mcu::failAt (void const* pc, void const* lr) {
    printf("failAt %p %p\n", pc, lr);
    for (uint32_t i = 0; i < systemClock() >> 15; ++i) {}
    systemReset();
}

int main () {
    fastClock();

    Serial<Uart> serial (2, "A2:PU7,A15:PU3");

    Printer printer (&serial, [](void* obj, uint8_t const* ptr, int len) {
        ((Serial<Uart>*) obj)->write(ptr, len);
    });
    stdOut = &printer;

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
