#include "hall.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

namespace gpio {
    extern uintptr_t base;

    template <typename TYPE, uint32_t ADDR>
        struct IoBase {
            constexpr static auto addr = ADDR;

            auto& operator() (int off) const {
                __sync_synchronize();
                return *(volatile TYPE*) (ADDR+off+base);
            }
        };

    template <uint32_t ADDR> using Io8 = IoBase<uint8_t,ADDR>;
    template <uint32_t ADDR> using Io16 = IoBase<uint16_t,ADDR>;

    template <uint32_t ADDR>
        struct Io32 : IoBase<uint32_t,ADDR> {
            using IoBase<uint32_t,ADDR>::operator();

            auto operator() (int off, int bit, uint8_t num =1) const {
                struct IOMask {
                    int o; uint8_t b, w;

                    operator uint32_t () const {
                        auto& a = *(volatile uint32_t*) (ADDR+o+base);
                        auto m = (1<<w)-1;
                        return (a >> b) & m;
                    }
                    auto operator= (uint32_t v) const {
                        auto& a = *(volatile uint32_t*) (ADDR+o+base);
                        auto m = (1<<w)-1;
                        a = (a & ~(m<<b)) | ((v & m)<<b);
                        return v & m;
                    }
                };

                return IOMask {off + (bit >> 5), (uint8_t) (bit & 0x1F), num};
            }
        };

    struct Pin {
        uint8_t port :4, pin :4;

        constexpr Pin () : port (15), pin (0) {}

        auto read () const { return GPIO(LEV0, pin); }
        void write (int v) const { GPIO(v ? SET0 : CLR0, pin) = 1; }

        // shorthand
        void toggle () const { write(!read()); }
        operator int () const { return read(); }
        void operator= (int v) const { write(v); }

        // pin definition string: [A-O][<pin#>]:[AFPO][DU][LNHV][<alt#>][,]
        // return -1 on error, 0 if no mode set, or the mode (always > 0)
        auto config (char const*) -> int;

        // define multiple pins, return nullptr if ok, else ptr to error
        static auto define (char const*, Pin* =nullptr, int =0) -> char const*;
        private:
        enum { LEV0=0x34, SET0=0x1C, CLR0=0x28 };
        constexpr static Io32<0> GPIO {};

        auto mode (int) const -> int;
        auto mode (char const* desc) const -> int;
    };
}

using namespace gpio;
using namespace hall;
using namespace boss;

auto Pin::config (char const*) -> int {
    enum { SEL0=0x00, OUT=0x1 };
    pin = 12;
    GPIO(SEL0+4, 3*2, 3) = OUT; // TODO careful with 3-bit fields, don't span word boundaries
    return 0; // TODO
}

#if DOCTEST
#include <doctest.h>

uintptr_t gpio::base;

constexpr auto size = 0x0100'0000;

void initMap () {
#if 0
    uint32_t info [6];

    {
        auto fd = open("/proc/device-tree/soc/ranges", O_RDONLY);
        CHECK(fd >= 0);
        auto n = read(fd, info, sizeof info);
        close(fd);
        CHECK(n >= 8);
    }

    if (info[0] == 0 && n >= 16) { // Pi 4
        info[0] = info[2];
        info[1] = info[3];
    }

    uint32_t addr = __builtin_bswap32(info[0]);
    uint32_t size = __builtin_bswap32(info[1]);

    printf("%08x %08x\n", addr, size);
#endif

    auto fd = open("/dev/gpiomem", O_RDWR | O_SYNC);
    CHECK(fd >= 0);

    base = (uintptr_t) mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    CHECK((void*) base != MAP_FAILED);

    printf("base %08x\n", base);
}

void deinitMap () {
    auto e = munmap((void*) base, size);
    CHECK(e == 0);
}

Pin led;

TEST_CASE("gpio") {
    initMap();

    systick::init();
    uint8_t mem [5000];
    pool::init(mem, sizeof mem);

    led.config("P12:P");
    led = 1;

    for (int n = 1; n < 10; ++n) {
        for (volatile int i = 0; i < 10'000'000; ++i) {}
        led.toggle();
    }

    Fiber::create([](void*) {
        for (int i = 0; i < 10; ++i) {
            led = 1;
            Fiber::msWait(100);
            led = 0;
            Fiber::msWait(400);
        }
    });

#if 0 // FIXME crashes ...
    while (Fiber::runLoop())
        idle();
#endif

    deinitMap();
}

#endif // DOCTEST
