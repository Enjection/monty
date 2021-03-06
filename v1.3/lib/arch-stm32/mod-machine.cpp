#include <monty.h>
#include "arch.h"
#include "jee-rf69.h"

#include <cassert>
#include <unistd.h>

using namespace monty;

#if !HAS_ARRAY
using Array = Bytes; // TODO not really the same thing, but best available?
#endif

//CG: module machine

// machine.pins.B3 = "PL"   : set mode of PB3 to push-pull, low speed
// machine.pins.B3 = 1      : set output pin to 1
// if machine.pins.B3: ...  : get input pin state

struct Pins : Object {
    static Type info;
    auto type () const -> Type const& override { return info; }

    auto attr (Value name, Value&) const -> Value override {
        char const* s = name;
        assert(strlen(s) >= 2);
        jeeh::Pin p (s[0], atoi(s+1));
        return p.read();
    }

    auto setAt (Value arg, Value val) -> Value override {
        assert(arg.isStr() && strlen(arg) >= 2);
        jeeh::Pin p (arg[0], atoi((char const*) arg + 1));
        if (val.isInt())
            p.write(val);
        else {
            assert(val.isStr());
            if (!p.mode((char const*) val))
                return {E::ValueError, "invalid pin mode", val};
        }
        return {};
    }
};

Type Pins::info (Q(0,"<pins>"));

static Pins pins; // there is one static pins object, used via attr access

// spi = machine.spi("A4,A5,A6,A7")
// spi.enable()
// x = spi.xfer(123)
// spi.disable()

struct Spi : Object, jeeh::SpiGpio {
    static Lookup const attrs;
    static Type info;
    auto type () const -> Type const& override { return info; }

    auto xfer (Value v) -> Value {
        assert(v.isInt());
        return transfer(v);
    }

    //CG: wrap Spi enable disable xfer
    auto enable () -> Value { jeeh::SpiGpio::enable(); return {}; }
    auto disable () -> Value { jeeh::SpiGpio::disable(); return {}; }
};

//CG: wrappers Spi

Type Spi::info (Q(0,"<spi>"), &Spi::attrs);

struct RF69 : Object, jeeh::RF69<jeeh::SpiGpio> {
    static Lookup const attrs;
    static Type info;
    auto type () const -> Type const& override { return info; }

    auto recv (ArgVec const& args) -> Value {
        assert(args.size() == 2);
        auto& a = args[1].asType<Array>();
        assert(a.size() >= 4);
        auto r = receive(a.begin()+4, a.size()-4);
        a[0] = rssi;
        a[1] = lna;
        a[2] = afc;
        a[3] = afc >> 8;
        return r+4;
    }

    auto xmit (ArgVec const& args) -> Value {
        assert(args.size() == 2 && args[0].isInt());
        auto& a = args[1].asType<Array>();
        send(args[0], a.begin(), a.size());
        return {};
    }

    auto sleep () -> Value {
        jeeh::RF69<jeeh::SpiGpio>::sleep();
        return {};
    }

    //CG: wrap RF69 recv xmit sleep
};

//CG: wrappers RF69

Type RF69::info (Q(0,"<rf69>"), &RF69::attrs);

//CG1 bind spi arg:s
static auto f_spi (char const* arg) -> Value {
    auto spi = new Spi;
    auto err = jeeh::Pin::define(arg, &spi->_mosi, 4);
    if (err != nullptr || !spi->isValid())
        return {E::ValueError, "invalid SPI pin", err};
    spi->init();
    return spi;
}

//CG1 bind rf69 pins:s node:i group:i freq:i
static auto f_rf69 (char const* pins, int node, int group, int freq) -> Value {
    auto rf69 = new RF69;
    auto err = jeeh::Pin::define(pins, &rf69->spi._mosi, 4);
    if (err != nullptr || !rf69->spi.isValid())
        return {E::ValueError, "invalid SPI pin", err};
    rf69->spi.init();
    rf69->init(node, group, freq);
    return rf69;
}

void enableSysTick (uint32_t divider) {
    VTableRam().systick = []() { ++ticks; };
    constexpr static uint32_t tick = 0xE000E010;
    MMIO32(tick+0x04) = MMIO32(tick+0x08) = divider - 1;
    MMIO32(tick+0x00) = 7;
}

auto monty::nowAsTicks () -> uint32_t {
    return ticks;
}

static auto msNow () -> Value {
    uint32_t t = ticks;
    static uint32_t begin;
    if (begin == 0)
        begin = t;
    return t - begin; // make all runs start out the same way
}

static Event tickEvent;
static int ms, tickerId;
static uint32_t start, last;

//CG1 bind ticker ? arg:i
static auto f_ticker (ArgVec const& args, int arg) -> Value {
    if (args.size() > 0) {
        ms = arg;
        start = msNow(); // set first timeout relative to now
        last = 0;
        tickerId = tickEvent.regHandler();
        assert(tickerId > 0);

        VTableRam().systick = []() {
            ++ticks;
            uint32_t t = msNow(); // TODO messy
            if (ms > 0 && (t - start) / ms != last) {
                last = (t - start) / ms;
                Context::setPending(tickerId);
            }
        };
    } else {
        VTableRam().systick = []() {
            ++ticks;
        };

        tickEvent.deregHandler();
        tickEvent.clear();
        assert(tickerId > 0);
    }
    return tickEvent;
}

//CG1 bind ticks
static auto f_ticks () -> Value {
    return msNow();
}

//CG1 bind cycles *
static auto f_cycles (ArgVec const& args) -> Value {
    (void) args; // not used, but any number of args accepted
    return jeeh::DWT::count() & 0x3FFFFFFF; // keep it positive in Value
}

//CG1 bind dog ? arg:i
static auto f_dog (ArgVec const& args, int arg) -> Value {
    static Iwdg dog;
    dog.reload(args.size() > 0 ? arg : 4095);
    return {};
}

//CG1 bind kick
static auto f_kick () -> Value {
    Iwdg::kick();
    return {};
}

//CG1 wrappers
static Lookup::Item const machine_map [] = {
    { Q(0,"pins"), pins },
};

//CG: module-end
