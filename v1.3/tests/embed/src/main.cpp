#define DOCTEST_CONFIG_COLORS_NONE
#define DOCTEST_CONFIG_NO_MULTI_LANE_ATOMICS
#define DOCTEST_CONFIG_NO_POSIX_SIGNALS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctestx.h"

TEST_CASE("smoke test") {
    CHECK(42 == 40 + 2);
}
