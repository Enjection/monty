#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace hall {
    void idle();
    [[noreturn]] void systemReset ();

    namespace systick {
        void init (uint8_t ms =100);
        auto millis () -> uint32_t;
    }
}
