#include <monty.h>

#include <cassert>
#include <ctime>

using namespace monty;

//CG: module machine

Event tickEvent;
int ms, tickerId;
uint32_t start, last;

static auto micros () -> uint64_t {
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC, &tv);
    return tv.tv_sec * 1000000LL + tv.tv_nsec / 1000; // µs resolution
}

auto monty::nowAsTicks () -> uint32_t {
    return micros() / 1000;
}

static auto msNow () -> Value {
    uint64_t us = micros();
    static uint64_t begin;
    if (begin == 0)
        begin = us;
    return (us - begin) / 1000; // make all runs start out the same way
}

// simulate in software, see INNER_HOOK in arch.h and monty/pyvm.h
void timerHook () {
    uint32_t t = msNow();
    if (ms > 0 && (t - start) / ms != last) {
        last = (t - start) / ms;
        if (tickerId > 0)
            Stacklet::setPending(tickerId);
    }
}

//CG1 bind ticker ? arg:i
static auto f_ticker (ArgVec const& args, int arg) -> Value {
    if (args.size() > 0) {
        ms = arg;
        start = msNow(); // set first timeout relative to now
        last = 0;
        tickerId = tickEvent.regHandler();
        assert(tickerId > 0);
    } else {
        tickEvent.deregHandler();
        tickEvent.clear();
        tickerId = 0;
    }

    return tickEvent;
}

//CG1 bind ticks
static auto f_ticks () -> Value {
    return msNow();
}

//CG1 wrappers
static Lookup::Item const machine_map [] = {
};

//CG: module-end
