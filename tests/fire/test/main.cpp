#include <hall.h>
#include <unity.h>

void setUp () {
}

void tearDown () {
}

void smokeTest () {
    TEST_ASSERT_EQUAL(42, 40 + 2);
}

int main () {
    UNITY_BEGIN();

    RUN_TEST(smokeTest);

    UNITY_END();
}
