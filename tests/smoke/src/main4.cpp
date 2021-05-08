#include <stm32l4xx.h>

void delayLoop (int n) {
    for (int i = 0; i < n; ++i)
        asm ("wfi"); // sleep until next interrupt
}

extern "C" void SysTick_Handler(void) {}

// USART TX on PA2 (115200 baud, 8N1)
void initUart () {
    RCC->APB1ENR1 |= RCC_APB1ENR1_USART2EN; // enable USART2
    RCC->AHB2ENR  |= RCC_AHB2ENR_GPIOAEN;   // enable GPIOA
    GPIOA->MODER  &= ~(3 << (2*2));         // clear mode
    GPIOA->MODER  |= 2 << (2*2);            // set PA2 as output
    GPIOA->AFR[0] |= 0x7 << (2*4);          // alternate mode 7
    USART2->BRR = 4'000'000 / 115'200;      // 115200 baud
    USART2->CR1 |= USART_CR1_TE|USART_CR1_UE; // enable UART send
}

void outCh (int ch) {
    while ((USART2->ISR & USART_ISR_TXE) == 0) {} // wait until empty
    USART2->TDR = ch;                       // send next char
}

int main () {
    initUart();

    // set 1000 Hz tick rate, i.e. every millisecond
    SysTick_Config(SystemCoreClock / 1000);

    
    for (int i = 0; true; ++i) {
        delayLoop(100);
        outCh('0' + i % 10);
    }
}
