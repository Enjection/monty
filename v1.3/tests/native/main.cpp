#include <doctest.h>

TEST_CASE("smoke test") {
    CHECK(42 == 40 + 2);
}

int main(int argc, char** argv) {
    return doctest::Context(argc, argv).run();
}
