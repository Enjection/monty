#include "boss.h"
#include <cstdio>
#include <cstdlib>

namespace hall {
    void idle();
    [[noreturn]] void systemReset ();

    struct Device {
        uint8_t _id;

        Device ();

        virtual void interrupt () { pending |= 1<<_id; }

        static auto dispatch () -> bool;
        static volatile uint32_t pending;
    };

    namespace systick {
        void init (uint8_t ms =100);
        void deinit ();
        auto millis () -> uint32_t;
        auto micros () -> uint16_t;
    }
}
