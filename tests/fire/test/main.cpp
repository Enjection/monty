#include <hall.h>
#include <unity.h>

using namespace hall;

void setUp () {
    systick::init();
}

void tearDown () {
    systick::deinit();
}

void smokeTest () {
    TEST_ASSERT_EQUAL(42, 40 + 2);
}

void ticking () {
    auto t = systick::millis();
    idle();
    TEST_ASSERT_EQUAL(t+1, systick::millis());
    idle();
    TEST_ASSERT_EQUAL(t+2, systick::millis());
}

int main () {
    UNITY_BEGIN();

    RUN_TEST(smokeTest);
    RUN_TEST(ticking);

    UNITY_END();
}
