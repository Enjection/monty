#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

TEST_CASE("smoke test") {
    CHECK(42 == 40 + 2);
}
