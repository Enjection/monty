#include "hall.h"

using namespace hall;

// USART TX on PA2 (115200 baud, 8N1), see RM0394 Rev 4
void initUart () {
    Pin::define("A2:P7");

    enum { APB1ENR1=0x58,CR1=0x00,BRR=0x0C }; // p.220 p.1238 p.1249
    dev::RCC(APB1ENR1, 17) = 1;               // USART2EN
    dev::USART2(BRR) = 4'000'000 / 115'200;   // 115200 baud
    dev::USART2(CR1) |= (1<<3) | (1<<0);      // TE UE
}

void outCh (void*, int ch) {
    enum { ISR=0x1C,TDR=0x28 };               // p.1253 p.1259
    while (dev::USART2(ISR, 7) == 0) {}       // wait until TXE
    dev::USART2(TDR) = ch;                    // send next char
}

extern "C" int printf (const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    auto r = veprintf(outCh, nullptr, fmt, ap);
    va_end(ap);
    return r;
}

void boss::debugf (const char*, ...) __attribute__((alias ("printf")));

int main () {
    systick::init();
    uint8_t mem [5000];
    pool::init(mem, sizeof mem);
    initUart();

    Fiber::create([](void*) {
        Pin led;
        led.config("B3:P");

        while (true) {
            led = 1;
            Fiber::msWait(100);
            led = 0;
            Fiber::msWait(400);
            printf("%u\n", systick::millis());
        }
    });

    while (Fiber::runLoop())
        idle();
}
