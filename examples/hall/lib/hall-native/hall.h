#include <cstdint>

namespace hall {
    auto systemHz () -> uint32_t;
    [[noreturn]] void systemReset ();
    void debugPutc (void*, int c);

    struct BlockIRQ {
        BlockIRQ () {}
        ~BlockIRQ () {}
    };

    struct Device {
        static void processAllPending () {}
    };
}
