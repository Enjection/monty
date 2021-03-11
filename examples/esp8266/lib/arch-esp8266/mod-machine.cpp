#include <monty.h>
#include "esp_common.h"

#include <cassert>
#include <unistd.h>

//CG2 if dir extend
#define HAS_ARRAY 1
#include <extend.h>

using namespace monty;

#if !HAS_ARRAY
using Array = Bytes; // TODO not really the same thing, but best available?
#endif

//CG: module machine

static Event tickEvent;
static int ms, tickerId;
static uint32_t start, last;

static auto millis () -> uint32_t {
    return (xTaskGetTickCount() * configTICK_RATE_HZ) / 1000;
}

static auto msNow () -> Value {
    uint32_t t = millis();
    static uint32_t begin;
    if (begin == 0)
        begin = t;
    return t - begin; // make all runs start out the same way
}

//CG1 bind ticker
static auto f_ticker (ArgVec const& args) -> Value {
    //CG: args ? arg:i
    if (arg > 0) {
        assert(args.size() == 1 && args[0].isInt());
        ms = arg;
        start = msNow(); // set first timeout relative to now
        last = 0;
        tickerId = tickEvent.regHandler();
        assert(tickerId > 0);

        // TODO
    } else {
        // TODO
        tickEvent.deregHandler();
        assert(tickerId > 0);
    }
    return tickEvent;
}

//CG1 bind ticks
static auto f_ticks (ArgVec const& args) -> Value {
    //CG: args
    return msNow();
}

//CG1 wrappers
static Lookup::Item const machine_map [] = {
};

//CG: module-end
