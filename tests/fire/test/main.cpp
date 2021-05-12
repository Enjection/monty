#include "hall.h"
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

void microsSame () {
    auto t1 = systick::micros();
    auto t2 = systick::micros();
    auto t3 = systick::micros();
    // might still fail, but the chance of that should be almost zero
    TEST_ASSERT(t1 == t2 || t2 == t3);
}

void microsChanged () {
    auto t = systick::micros();
    for (volatile int i = 0; i < 1000; ++i) {}
    TEST_ASSERT_NOT_EQUAL(t, systick::micros());
}

int main () {
    UNITY_BEGIN();

    RUN_TEST(smokeTest);
    RUN_TEST(ticking);
    RUN_TEST(microsSame);
    RUN_TEST(microsChanged);

    UNITY_END();
}
