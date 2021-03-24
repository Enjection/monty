namespace jeeh {
    struct Pin {
        uint8_t _port, _pin;
        
        constexpr Pin () : _port (0xFF), _pin (0xFF) {}
        constexpr Pin (char port, int pin) : _port (port-'A'), _pin (pin) {}

        auto gpio32 (int off) const -> volatile uint32_t& {
            return MMIO32(Periph::gpio + 0x400 * _port + off);
        }

        auto isValid () const -> bool {
            return _port < 16 && _pin < 16;
        }

#if STM32F1
        enum { crl=0x00, crh=0x04, idr=0x08, odr=0x0C, bsrr=0x10, brr=0x14 };

        void mode (int mval, int /*ignored*/ =0) const {
            // enable GPIOx and AFIO clocks
            MMIO32(Periph::rcc+0x18) |= (1 << _port) | (1<<0);

            if (mval == 0b1000 || mval == 0b1100) {
                uint16_t mask = 1 << _pin;
                gpio32(bsrr) = mval & 0b0100 ? mask : mask << 16;
                mval = 0b1000;
            }

            uint32_t cr = _pin & 8 ? crh : crl;
            int shift = 4 * (_pin & 7);
            gpio32(cr) = (gpio32(cr) & ~(0xF << shift)) | (mval << shift);
        }
#else
        enum { moder=0x00, typer=0x04, ospeedr=0x08, pupdr=0x0C, idr=0x10,
                odr=0x14, bsrr=0x18, afrl=0x20, afrh=0x24, brr=0x28 };

        void mode (int mval, int alt =0) const {
            // enable GPIOx clock
#if STM32F3
            Periph::bitSet(Periph::rcc+0x14, _port);
#elif STM32F4 | STM32F7
            Periph::bitSet(Periph::rcc+0x30, _port);
#elif STM32G0
            Periph::bitSet(Periph::rcc+0x34, _port);
#elif STM32H7
            Periph::bitSet(Periph::rcc+0xE0, _port);
#elif STM32L4
            Periph::bitSet(Periph::rcc+0x4C, _port);
#elif STM32L0
            Periph::bitSet(Periph::rcc+0x2C, _port);
#endif
            gpio32(moder) = (gpio32(moder) & ~(3 << 2*_pin))
                                    | ((mval>>3) << 2*_pin);
            gpio32(typer) = (gpio32(typer) & ~(1 << _pin))
                                | (((mval>>2)&1) << _pin);
            gpio32(pupdr) = (gpio32(pupdr) & ~(3 << 2*_pin))
                                     | ((mval&3) << 2*_pin);
            gpio32(ospeedr) = (gpio32(ospeedr) & ~(3 << 2*_pin))
                                             | (0b11 << 2*_pin);
            auto afr = _pin & 8 ? afrh : afrl;
            int shift = 4 * (_pin & 7);
            gpio32(afr) = (gpio32(afr) & ~(0xF << shift)) | (alt << shift);
        }
#endif

        auto read () const -> int {
            return (gpio32(idr) >> _pin) & 1;
        }

        void write (int v) const {
            auto mask = 1 << _pin;
            gpio32(bsrr) = v ? mask : mask << 16;
        }

        // shorthand
        void toggle () const { write(read() ^ 1); }
        operator int () const { return read(); }
        void operator= (int v) const { write(v); }

        // mode string: [AFDUPO][LNHV][<n>][,]
        auto mode (char const* desc) const -> bool {
            int a = 0, m = 0;
            for (auto s = desc; *s != ',' && *s != 0; ++s)
                switch (*s) {
                    case 'A': m = (int) Pinmode::in_analog; break;
                    case 'F': m = (int) Pinmode::in_float; break;
                    case 'D': m = (int) Pinmode::in_pulldown; break;
                    case 'U': m = (int) Pinmode::in_pullup; break;

                    case 'P': m = (int) Pinmode::out; break; // push-pull
                    case 'O': m = (int) Pinmode::out_od; break;

                    case 'L': m = m & 0x1F; break;          // low speed
                    case 'N': break;                        // normal speed
                    case 'H': m = (m & 0x1F) | 0x40; break; // high speed
                    case 'V': m = m | 0x60; break;          // very high speed

                    default:  if (*s < '0' || *s > '9' || a > 1)
                                  return false;
                              m |= 0x10; // alt mode
                              a = 10 * a + *s - '0';
                    case ',': break; // valid as terminator
                }
            mode(m, a);
            return true;
        }

        // pin definition string: [A-P][<n>]:[<mode>][,]
        static auto define (char const* desc) -> Pin {
            if (desc != nullptr) {
                uint8_t port = *desc++;
                uint8_t pin = 0;
                while ('0' <= *desc && *desc <= '9')
                    pin = 10 * pin + *desc++ - '0';
                Pin p (port, pin);
                if (p.isValid() && (*desc++ != ':' || p.mode(desc)))
                    return p;
            }
            return {};
        }

        // define multiple pins, returns ptr to error or nullptr
        static auto define (char const* d, Pin* v, int n) -> char const* {
            while (--n >= 0) {
                *v = define(d);
                if (!v->isValid())
                    break;
                ++v;
                auto p = strchr(d, ',');
                if (n == 0)
                    return *d != 0 ? p : nullptr; // point to comma if more
                if (p == nullptr)
                    break;
                d = p+1;
            }
            return d;
        }
    };

    struct SpiGpio {
        Pin _mosi, _miso, _sclk, _nsel;
        uint8_t _cpol =0;

        constexpr SpiGpio () {}
        constexpr SpiGpio (Pin mo, Pin mi, Pin ck, Pin ss, int cp =0)
            : _mosi (mo), _miso (mi), _sclk (ck), _nsel (ss), _cpol (cp) {}

        void init () {
            _nsel.mode((int) Pinmode::out); disable();
            _sclk.mode((int) Pinmode::out); _sclk = _cpol;
            _miso.mode((int) Pinmode::in_float);
            _mosi.mode((int) Pinmode::out);
        }

        auto isValid () const -> bool {
            return _mosi.isValid() && _nsel.isValid(); // check first & last
        }

        void enable () const { _nsel = 0; }
        void disable () const { _nsel = 1; }

        auto transfer (uint8_t v) const -> uint8_t {
            for (int i = 0; i < 8; ++i) {
                _mosi = v & 0x80;
                v <<= 1;
                _sclk = !_cpol;
                v |= _miso;
                _sclk = _cpol;
            }
            return v;
        }

        void transfer (uint8_t* buf, int len) const {
            enable();
            for (int i = 0; i < len; ++i)
                buf[i] = transfer(buf[i]);
            disable();
        }

        void send (uint8_t const* buf, int len) const {
            enable();
            for (int i = 0; i < len; ++i)
                transfer(buf[i]);
            disable();
        }

        void receive (uint8_t* buf, int len) const {
            enable();
            for (int i = 0; i < len; ++i)
                buf[i] = transfer(0);
            disable();
        }
    };

    // cycle counts, see https://stackoverflow.com/questions/11530593/
    // TODO remove from JeeH's F1/F3/H7 - looks like they're all the same

    namespace DWT {
        constexpr uint32_t dwt   = 0xE0001000;
        enum { ctrl=0x0, cyccnt=0x4, lar=0xFB0 };

        static void init () {
            constexpr uint32_t scb_demcr = 0xE000EDFC;
            MMIO32(dwt+lar) = 0xC5ACCE55;
            MMIO32(scb_demcr) |= (1<<24); // set TRCENA in DEMCR
        }

        // return count since previous call (1st result is bogus)
        inline auto count () -> uint32_t {
            auto n = MMIO32(dwt+cyccnt);
            init();
            MMIO32(dwt+ctrl) |= 1<<0;
            MMIO32(dwt+cyccnt) = 0;
            return n;
        }
    };
}
