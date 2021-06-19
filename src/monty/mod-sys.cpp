// sys.cpp - the sys module

#include <monty.h>
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

//CG< wrappers
static Function const fo_gc ("", (Function::Prim) f_gc);

static Lookup::Item const sys_map [] = {
    { Q(171,"gc"), fo_gc },
//CG>
    { Q(173,"ready"), Context::ready },
    { Q(174,"modules"), Module::loaded },
    { Q(63,"builtins"), Module::builtins },
    { Q(175,"implementation"), Q(176,"monty") },
    { Q(177,"version"), VERSION },
};

//CG2 module-end
static Lookup const sys_attrs (sys_map);
Module ext_sys (Q(172,"sys"), sys_attrs);
