#include <cstdint>

void delayLoop (int n) {
#if STM32
    // empirically tweaked for L432's 4 MHz startup clock rate
    for (int i = 0; i < n * 800; ++i)
        asm ("");
#endif
}

int main () {
    // see RM0394 Rev 4
    const auto RCC   = (volatile uint32_t*) 0x4002'1000; // p.68
    const auto GPIOB = (volatile uint32_t*) 0x4800'0400; // p.68
    enum { AHB2ENR=0x4C };        // p.244
    enum { MODER=0x00,ODR=0x14 }; // pp.274

    RCC[AHB2ENR/4] |= (1<<1);     // GPIOBEN, p.244
    GPIOB[MODER/4] &= ~(0b11<<6); // clear bits 6..7
    GPIOB[MODER/4] |= (0b01<<6);  // output mode, p.267

    while (true) {
        GPIOB[ODR/4] |= 1<<3;
        delayLoop(100); // ≈ 0.1s on
        GPIOB[ODR/4] &= ~(1<<3);
        delayLoop(400); // ≈ 0.4s off
    }
}
