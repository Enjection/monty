#include <cstdint>

namespace hall {
    enum struct STM { F1, F4, F7, L0, L4 };
#if STM32F1
    constexpr auto FAMILY = STM::F1;
#elif STM32F4
    constexpr auto FAMILY = STM::F4;
#elif STM32F7
    constexpr auto FAMILY = STM::F7;
#elif STM32L0
    constexpr auto FAMILY = STM::L0;
#elif STM32L4
    constexpr auto FAMILY = STM::L4;
#endif

    void idle () __attribute__ ((weak)); // called with interrupts disabled

    auto fastClock (bool pll =true) -> uint32_t;
    auto slowClock (bool low =true) -> uint32_t;
    auto systemHz () -> uint32_t;
    [[noreturn]] void systemReset ();
    void debugPutc (void*, int c);

    struct IOWord {
        const uint32_t addr;

        operator uint32_t () const {
            return *(volatile uint32_t*) addr;
        }
        auto operator= (uint32_t v) const -> uint32_t {
            *(volatile uint32_t*) addr = v; return v;
        }
        
        auto mask (int b, uint8_t w =1) const {
            struct IOMask {
                volatile uint32_t& a; uint8_t b, w;

                operator uint32_t () const {
                    auto m = (1<<w)-1;
                    return (a >> b) & m;
                }
                auto operator= (uint32_t v) const {
                    auto m = (1<<w)-1;
                    a = (a & ~(m<<b)) | ((v & m)<<b);
                    return v & m;
                }
            };

            return IOMask {((volatile uint32_t*) addr)[b>>5], b & 0x1F, w};
        }

        auto& operator[] (int b) const {
            if constexpr (FAMILY == STM::L0 || FAMILY == STM::F7)
                return mask(b);
            // FIXME bit-band only works for specific RAM and periperhal areas!
            return *(volatile uint32_t*)
                ((addr & 0xF000'0000) + 0x0200'0000 + (addr << 5) + (b << 2));
        }
    };

    template <uint32_t ADDR>
    constexpr auto io32 (int off) { return IOWord {ADDR+off}; }

    template <uint32_t ADDR>
    constexpr auto& io16 (int off) { return *(volatile uint16_t*) (ADDR+off); }

    template <uint32_t ADDR>
    constexpr auto& io8 (int off) { return *(volatile uint8_t*) (ADDR+off); }

#if STM32L4
    constexpr auto RCC   = io32<0x4002'1000>;
    constexpr auto GPIOA = io32<0x4800'0000>;
#endif
    constexpr auto NVIC  = io32<0xE000'E100>;

    struct Pin {
        uint8_t _port :4, _pin :4;

        constexpr Pin () : _port (15), _pin (0) {}

        auto read () const { return (gpio32(IDR)>>_pin) & 1; }
        void write (int v) const { gpio32(BSRR) = (v ? 1 : 1<<16)<<_pin; }

        // shorthand
        void toggle () const { write(read() ^ 1); }
        operator int () const { return read(); }
        void operator= (int v) const { write(v); }

        // pin definition string: [A-O][<pin#>]:[AFPO][DU][LNHV][<alt#>][,]
        // return -1 on error, 0 if no mode set, or the mode (always > 0)
        auto config (char const*) -> int;

        // define multiple pins, return nullptr if ok, else ptr to error
        static auto define (char const*, Pin* =nullptr, int =0) -> char const*;
    private:
        constexpr static auto OFF = FAMILY == STM::F1 ? 0x0 : 0x8;
        enum { IDR=0x08+OFF, BSRR=0x10+OFF };

        auto gpio32 (int off) const -> IOWord { return GPIOA(0x400*_port+off); }
        auto mode (int) const -> int;
        auto mode (char const* desc) const -> int;
    };

    struct BlockIRQ {
        BlockIRQ () { asm ("cpsid i"); }
        ~BlockIRQ () { asm ("cpsie i"); }
    };

    struct Device {
        uint8_t _id;

        Device ();

        void irqInstall (uint32_t irq) const;

        virtual void interrupt () { pending |= 1<<_id; }
        virtual void process () {}
        virtual void expire (uint16_t, uint16_t&) {}

        static void nvicEnable (uint8_t irq) {
            NVIC(0x00+4*(irq>>5)) = 1 << (irq & 0x1F);
        }

        static void nvicDisable (uint8_t irq) {
            NVIC(0x80+4*(irq>>5)) = 1 << (irq & 0x1F);
        }

        static volatile uint32_t pending;
    };

    void processAllPending ();

    namespace systick {
        void init (uint8_t ms =100);
        void deinit ();
        auto millis () -> uint32_t;
        auto micros () -> uint32_t;
    }

    namespace cycles {
        constexpr auto DWT = io32<0xE000'1000>;

        void init ();
        void deinit ();
        inline void clear () { DWT(0x04) = 0; }
        inline auto count () -> uint32_t { return DWT(0x04); }
    }

    namespace watchdog {  // [1] pp.495
        auto resetCause () -> int; // watchdog: -1, nrst: 1, power: 2, other: 0
        void init (int rate =6);   // max timeout, 0 ≈ 500 ms, 6 ≈ 32 s
        void reload (int n);       // 0..4095 x 125 µs (0) .. 8 ms (6)
        void kick ();
    }

    namespace rtc {
        struct DateTime { uint8_t yr, mo, dy, hh, mm, ss; };

        void init ();
        auto get () -> DateTime;
        void set (DateTime dt);
        auto getData (int reg) -> uint32_t;
        void setData (int reg, uint32_t val);
    }
}
