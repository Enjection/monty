#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

TEST_CASE("smoke test") {
    CHECK(42 == 40 + 2);
}

//FIXME this shouldn't be here!!!
#include <monty.h>
auto monty::vmImport (char const*) -> uint8_t const* { return nullptr; }
