#include <cstdint>
#include <cstring>

extern "C" int printf(char const* fmt, ...);

namespace monty {
    //CG1 version
    constexpr auto VERSION = "<stripped>";
}

#include "vecs.h"
#include "objs.h"
#include "dash.h"
#include "pyvm.h"
