// sys.cpp - the sys module

#include "monty.h"
#include <cassert>

using namespace Monty;

//CG1 VERSION
constexpr auto VERSION = Q(192,"v0.96");

static auto f_suspend (ArgVec const& args) -> Value {
    assert(args.num == 1 && args[0].isInt());
    int id = args[0];
    if (id >= 0)
        Interp::suspend(id);
    return {};
}

static auto f_gcavail (ArgVec const& args) -> Value {
    assert(args.num == 0);
    return gcAvail();
}

static auto f_gcnow (ArgVec const& args) -> Value {
    assert(args.num == 0);
    gcNow();
    return {};
}

static List gcdata; // keep this around to avoid reallocating on each call

static auto f_gcstats (ArgVec const& args) -> Value {
    assert(args.num == 0);
    constexpr auto NSTATS = sizeof gcStats / sizeof (uint32_t);
    if (gcdata.size() != NSTATS) {
        gcdata.remove(0, gcdata.size());
        gcdata.insert(0, NSTATS);
    }
    for (uint32_t i = 0; i < NSTATS; ++i)
        gcdata.setAt(i, ((uint32_t const*) &gcStats)[i]);
    return gcdata;
}

static Function const fo_suspend (f_suspend);
static Function const fo_gcavail (f_gcavail);
static Function const fo_gcnow (f_gcnow);
static Function const fo_gcstats (f_gcstats);

static Lookup::Item const lo_sys [] = {
    { Q(193,"tasks"), Interp::tasks },
    { Q(194,"modules"), Interp::modules },
    { Q(195,"suspend"), fo_suspend },
    { Q(196,"gc_avail"), fo_gcavail },
    { Q(197,"gc_now"), fo_gcnow },
    { Q(198,"gc_stats"), fo_gcstats },
    { Q(199,"implementation"), Q(200,"monty") },
    { Q(201,"version"), VERSION },
};

static Lookup const ma_sys (lo_sys, sizeof lo_sys);
extern Module const m_sys (ma_sys);