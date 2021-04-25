#include <cstdint>

namespace hall {
    void idle () __attribute__ ((weak)); // called with interrupts disabled

    struct IOWord {
        uint32_t volatile& addr;

        operator uint32_t () const { return addr; }
        auto operator= (uint32_t v) const -> uint32_t { addr = v; return v; }
        
        struct IOMask {
            uint32_t volatile& addr;
            uint8_t bit, width;

            auto get () const -> uint32_t {
                auto mask = (1<<width)-1;
                return (addr >> bit) & mask;
            }
            void set (uint32_t v) const {
                auto mask = (1<<width)-1;
                addr = (addr & ~(mask<<bit)) | ((v & mask)<<bit);
            }

            // shorthand
            operator uint32_t () const { return get(); }
            auto operator= (uint32_t v) const { set(v); return v; }
        };

        auto mask (int b, uint8_t w =1) {
            return IOMask {(&addr)[b>>5], (uint8_t) (b & 0x1F), w};
        }

#if STM32L0 || STM32F7
        auto& at (int b) { return mask(b); }
#else
        // use bit-banding, only works for specific RAM and periperhal areas
        auto& at (int b) {
            auto a = (uint32_t) &addr;
            return *(uint32_t volatile*)
                ((a & 0xF000'0000) + 0x0200'0000 + (a << 5) + (b << 2));
        }
#endif

        // shorthand
        auto& operator[] (int b) { return at(b); }
    };

    template <uint32_t ADDR>
    auto io32 (int off =0) {
        return IOWord {*(uint32_t volatile*) (ADDR+off)};
    }
    template <uint32_t ADDR>
    auto& io16 (int off =0) {
        return *(uint16_t volatile*) (ADDR+off);
    }
    template <uint32_t ADDR>
    auto& io8 (int off =0) {
        return *(uint8_t volatile*) (ADDR+off);
    }

#if STM32L4
    constexpr auto RCC  = io32<0x4002'1000>;
    constexpr auto GPIO = io32<0x4800'0000>;
#endif
    constexpr auto SCB  = io32<0xE000'E000>;

    struct Pin {
        uint8_t _port :4, _pin :4;

        constexpr Pin () : _port (15), _pin (0) {}

#if STM32F1
        enum { IDR=0x08, ODR=0x0C, BSRR=0x10 };
#else
        enum { IDR=0x10, ODR=0x14, BSRR=0x18 };
#endif

        auto read () const { return (gpio32(IDR)>>_pin) & 1; }
        void write (int v) const { gpio32(BSRR) = (v ? 1 : 1<<16)<<_pin; }

        // shorthand
        void toggle () const { write(read() ^ 1); }
        operator int () const { return read(); }
        void operator= (int v) const { write(v); }

        // pin definition string: [A-O][<pin#>]:[AFPO][DU][LNHV][<alt#>][,]
        // return -1 on error, 0 if no mode set, or the mode (always > 0)
        auto define (char const*) -> int;

        // define multiple pins, return nullptr if ok, else ptr to error
        static auto define (char const*, Pin*, int) -> char const*;
    private:
        auto gpio32 (int off) const -> IOWord { return GPIO(0x400*_port+off); }
        auto mode (int) const -> int;
        auto mode (char const* desc) const -> int;
    };

    struct BlockIRQ {
        BlockIRQ () { asm ("cpsid i"); }
        ~BlockIRQ () { asm ("cpsie i"); }
    };

    void idle () __attribute__ ((weak)); // called with interrupts disabled

    auto systemClock () -> uint32_t;

    namespace systick {
        void init (uint8_t ms =100);
        void deinit ();
    }
}
