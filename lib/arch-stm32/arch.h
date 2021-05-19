namespace device {
#include "prelude.h"
}

namespace altpins {
#include "altpins.h"
}

#include <jee.h>
#include <jee-stm32.h>

//CG: includes

namespace arch {
    using namespace monty;
    using namespace device;
    using namespace altpins;

    auto cliTask () -> monty::Context*;

    void init (int =0);
    void idle ();
    void done [[noreturn]] ();

    struct Device : Event {
        Device () { regHandler(); }
        ~Device () override { deregHandler(); } // TODO clear device slot

        virtual void irqHandler () =0;

        // install the uart IRQ dispatch handler in the hardware IRQ vector
        void installIrq (uint32_t irq) {
            //assert(irq < sizeof irqMap);
            for (auto& e : devMap) {
                if (e == nullptr)
                    e = this;
                if (e == this) {
                    irqMap[16+irq] = &e - devMap;

                    // TODO could be hard-coded, no need for RAM-based irq vector
                    (&VTableRam().wwdg)[irq] = irqDispatch;

                    constexpr uint32_t NVIC_ENA = 0xE000E100;
                    MMIO32(NVIC_ENA+4*(irq/32)) = 1 << (irq%32);

                    return;
                }
            }
            //assert(false); // ran out of unused device slots
        }

        void trigger () {
            if (_id >= 0)
                Context::setPending(_id);
        }

        template< size_t N >
        static void configAlt (AltPins const (&map) [N], int pin, int dev) {
            auto n = findAlt(map, pin, dev);
            if (n > 0) {
                jeeh::Pin t ('A'+(pin>>4), pin&0xF);
                auto m = (int) Pinmode::alt_out | (int) Pinmode::in_pullup;
                t.mode(m, n); // TODO still using JeeH pinmodes
            }
        }

        static void reset [[noreturn]] () {
            // ARM Cortex specific
            MMIO32(0xE000ED0C) = (0x5FA<<16) | (1<<2); // SCB AIRCR reset
            while (true) {}
        }

        static void irqDispatch () {
            auto vecNum = MMIO8(0xE000ED04); // ICSR
            auto idx = irqMap[vecNum];
            devMap[idx]->irqHandler();
        }

        static uint8_t irqMap [16 + (uint8_t) IrqVec::limit];
        static Device* devMap [20]; // large enough to handle all device objects

        static_assert(sizeof devMap / sizeof *devMap < 256, "doesn't fit in irqMap");
    };

    struct Ticker : Device {
        Ticker () { installIrq(-1); } // SysTick

        void irqHandler () override {
            ++ticks;
            trigger();
        }

        auto next () -> Value override {
            auto t = triggerExpired(ticks);
            if (t == 0)
                deregHandler(); // TODO bit of a hack to clean up this way
            return {};
        }
    };

// see https://interrupt.memfault.com/blog/arm-cortex-m-exceptions-and-nvic

#if STM32F1
#include "uart-f1.h"
#elif STM32F4 || STM32F7
#include "uart-f4.h"
#elif STM32L0
#include "uart-l0.h"
#elif STM32L4
#include "uart-l4.h"
#endif
}
