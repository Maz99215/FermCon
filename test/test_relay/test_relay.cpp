#include <Arduino.h>
#include <RelayController.h>
#include <unity.h>

RelayController relay;

void setUp(void) {
    relay.begin();
}

void tearDown(void) {
    relay.allOff();
}

void test_initial_state() {
    TEST_ASSERT_FALSE(relay.isCoolOn());
    TEST_ASSERT_FALSE(relay.isHeatOn());
}

void test_set_cool() {
    relay.setCool(true);
    TEST_ASSERT_TRUE(relay.isCoolOn());
    TEST_ASSERT_FALSE(relay.isHeatOn());
}

void test_set_heat() {
    relay.setHeat(true);
    TEST_ASSERT_FALSE(relay.isCoolOn());
    TEST_ASSERT_TRUE(relay.isHeatOn());
}

void test_exclusivity() {
    relay.setCool(true);
    relay.setHeat(true);
    TEST_ASSERT_FALSE(relay.isCoolOn());
    TEST_ASSERT_TRUE(relay.isHeatOn());
}

void test_all_off() {
    relay.setCool(true);
    relay.allOff();
    TEST_ASSERT_FALSE(relay.isCoolOn());
    TEST_ASSERT_FALSE(relay.isHeatOn());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_initial_state);
    RUN_TEST(test_set_cool);
    RUN_TEST(test_set_heat);
    RUN_TEST(test_exclusivity);
    RUN_TEST(test_all_off);
    UNITY_END();
    return 0;
}