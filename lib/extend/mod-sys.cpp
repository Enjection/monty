// sys.cpp - the sys module

#include <monty.h>
#include <cassert>

using namespace monty;

//CG: module sys

//CG1 bind gc
static auto f_gc () -> Value {
    Stacklet::gcAll();
    return {};
}

//CG1 bind gcmax
static auto f_gcmax () -> Value {
    return gcMax();
}

//CG1 bind gcstats
static auto f_gcstats () -> Value {
    auto data = new List;
    for (auto e : gcStats.v)
        data->append(e);
    return data;
}

//CG< wrappers
static Function const fo_gc ("", (Function::Prim) f_gc);
static Function const fo_gcmax ("", (Function::Prim) f_gcmax);
static Function const fo_gcstats ("", (Function::Prim) f_gcstats);

static Lookup::Item const sys_map [] = {
    { Q(171,"gc"), fo_gc },
    { Q(172,"gcmax"), fo_gcmax },
    { Q(173,"gcstats"), fo_gcstats },
//CG>
    { Q(175,"ready"), Stacklet::ready },
    { Q(176,"modules"), Module::loaded },
    { Q(63,"builtins"), Module::builtins },
    { Q(177,"implementation"), Q(178,"monty") },
    { Q(179,"version"), VERSION },
};

//CG2 module-end
static Lookup const sys_attrs (sys_map);
Module ext_sys (Q(174,"sys"), sys_attrs);
