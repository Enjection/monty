#include <stm32l4xx.h>

void delayLoop (int n) {
    for (int i = 0; i < n; ++i)
        asm ("wfi"); // sleep until next interrupt
}

extern "C" void SysTick_Handler(void) {}

// LED on PB3
void initLed () {
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
    GPIOB->MODER &= ~GPIO_MODER_MODE3_Msk;
    GPIOB->MODER |= GPIO_MODER_MODE3_0;
}

int main () {
    initLed();

    // set 1000 Hz tick rate, i.e. every millisecond
    SysTick_Config(SystemCoreClock / 1000);

    while (true) {
        GPIOB->ODR |= 1<<3;
        delayLoop(100);
        GPIOB->ODR &= ~(1<<3);
        delayLoop(400);
    }
}
