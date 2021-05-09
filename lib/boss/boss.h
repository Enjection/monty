#include "hall.h"
#include <cstdarg>

namespace boss {
    using namespace hall;

    auto veprintf(void(*)(void*,int), void*, char const* fmt, va_list ap) -> int;
}
