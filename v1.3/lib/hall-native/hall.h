#include "boss.h"
#include <cstdlib>
#include <cstdio>

namespace hall {
    void idle();
    [[noreturn]] void systemReset ();

    namespace systick {
        extern void (*expirer)(uint16_t,uint16_t&);

        void init (uint8_t ms =100);
        void deinit ();
        auto millis () -> uint32_t;
        auto micros () -> uint32_t;
    }
}
