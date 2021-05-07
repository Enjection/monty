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

Type Pins::info (Q(218,"<pins>"));

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

//CG< wrappers Spi
static   auto const  m_spi_disable = Method::wrap(&Spi::disable);
static Method const mo_spi_disable (m_spi_disable);
static   auto const  m_spi_enable = Method::wrap(&Spi::enable);
static Method const mo_spi_enable (m_spi_enable);
static   auto const  m_spi_xfer = Method::wrap(&Spi::xfer);
static Method const mo_spi_xfer (m_spi_xfer);

static Lookup::Item const spi_map [] = {
    { Q(204,"disable"), mo_spi_disable },
    { Q(205,"enable"), mo_spi_enable },
    { Q(206,"xfer"), mo_spi_xfer },
};
Lookup const Spi::attrs (spi_map);
//CG>

Type Spi::info (Q(219,"<spi>"), &Spi::attrs);

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

//CG< wrappers RF69
static   auto const  m_rf69_recv = Method::wrap(&RF69::recv);
static Method const mo_rf69_recv (m_rf69_recv);
static   auto const  m_rf69_sleep = Method::wrap(&RF69::sleep);
static Method const mo_rf69_sleep (m_rf69_sleep);
static   auto const  m_rf69_xmit = Method::wrap(&RF69::xmit);
static Method const mo_rf69_xmit (m_rf69_xmit);

static Lookup::Item const rf69_map [] = {
    { Q(207,"recv"), mo_rf69_recv },
    { Q(208,"sleep"), mo_rf69_sleep },
    { Q(209,"xmit"), mo_rf69_xmit },
};
Lookup const RF69::attrs (rf69_map);
//CG>

Type RF69::info (Q(220,"<rf69>"), &RF69::attrs);

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
                Stacklet::setPending(tickerId);
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

//CG< wrappers
static Function const fo_cycles ("*", (Function::Prim) f_cycles);
static Function const fo_dog ("?i", (Function::Prim) f_dog);
static Function const fo_kick ("", (Function::Prim) f_kick);
static Function const fo_rf69 ("siii", (Function::Prim) f_rf69);
static Function const fo_spi ("s", (Function::Prim) f_spi);
static Function const fo_ticker ("?i", (Function::Prim) f_ticker);
static Function const fo_ticks ("", (Function::Prim) f_ticks);

static Lookup::Item const machine_map [] = {
    { Q(210,"cycles"), fo_cycles },
    { Q(211,"dog"), fo_dog },
    { Q(212,"kick"), fo_kick },
    { Q(213,"rf69"), fo_rf69 },
    { Q(214,"spi"), fo_spi },
    { Q(215,"ticker"), fo_ticker },
    { Q(216,"ticks"), fo_ticks },
//CG>
    { Q(221,"pins"), pins },
};

//CG2 module-end
static Lookup const machine_attrs (machine_map);
Module ext_machine (Q(217,"machine"), machine_attrs);
