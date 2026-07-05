#include <Arduino.h>
#include <time_mock.h>   // remap time() -> mock_time() (via -DUNIT_TEST)
#include <FermentationInfo.h>
#include <unity.h>

FermentationInfo fermentation;

void setUp(void) {
    fermentation.begin();
    mock_resetMillis();
    // Force le chemin "fallback millis()" (pas de NTP) :
    // time(nullptr) renverra 0, donc < 1600000000 -> startBatch utilise millis().
    mock_setEpoch(0);
    fermentation.resetBatch();
}

void tearDown(void) {
    fermentation.resetBatch();
}

void test_stage_name() {
    fermentation.setStageName("Primaire");
    TEST_ASSERT_EQUAL_STRING("Primaire", fermentation.getStageName().c_str());
}

void test_initial_days() {
    TEST_ASSERT_EQUAL(0, fermentation.getFermentDays());
}

void test_days_after_start() {
    fermentation.startBatch();               // refMillis = millis() = 0
    mock_advanceMillis(2UL * 24UL * 3600UL * 1000UL); // ~2 jours
    TEST_ASSERT_EQUAL(2, fermentation.getFermentDays());
}

void test_reset_batch() {
    fermentation.startBatch();
    fermentation.resetBatch();
    TEST_ASSERT_EQUAL(0, fermentation.getFermentDays());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_stage_name);
    RUN_TEST(test_initial_days);
    RUN_TEST(test_days_after_start);
    RUN_TEST(test_reset_batch);
    return UNITY_END();
}
