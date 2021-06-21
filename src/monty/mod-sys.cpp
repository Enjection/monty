// sys.cpp - the sys module

#include "monty.h"
#include <cassert>

using namespace monty;

//CG: module sys

//CG1 bind gc
static auto f_gc () -> Value {
    Context::gcAll();
    return {};
}

#if 0
// CG1 bind gcmax
static auto f_gcmax () -> Value {
    return gcMax();
}

// CG1 bind gcstats
static auto f_gcstats () -> Value {
    auto data = new List;
    for (auto e : gcStats.v)
        data->append(e);
    return data;
}
#endif

//CG1 wrappers
static Lookup::Item const sys_map [] = {
    { Q(0,"ready"), Context::ready },
    { Q(0,"modules"), Module::loaded },
    { Q(0,"builtins"), Module::builtins },
    { Q(0,"implementation"), Q(0,"monty") },
    { Q(0,"version"), VERSION },
};

//CG: module-end
