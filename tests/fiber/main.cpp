#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "hall.h"

void boss::debugf (const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

TEST_CASE("smoke test") {
    CHECK(42 == 40 + 2);
}
