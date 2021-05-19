#if DOCTEST

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>

#else

#include <cstdarg>
#include <cstdio>

namespace boss {
    void debugf (const char* fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
    }
}

#endif // DOCTEST
