#include <Arduino.h>
#include <ProfileManager.h>
#include <unity.h>

// ProfileStep est une struct GLOBALE (définie dans ProfileManager.h),
// pas un type imbriqué : on l'utilise donc sans préfixe ProfileManager::

ProfileManager profile;

void setUp(void) {
    profile.clearSteps();
    mock_resetMillis();
}

void tearDown(void) {
    profile.stop();
}

// Un PALIER maintient tempStart
void test_palier() {
    ProfileStep step;
    step.type = ProfileStep::PALIER;
    step.tempStart = 18.0;
    step.tempEnd = 18.0;
    step.durationS = 3600;
    profile.addStep(step);
    profile.start();
    TEST_ASSERT_EQUAL_FLOAT(18.0, profile.getCurrentSetpoint());
}

// Une RAMPE interpole linéairement de tempStart à tempEnd
void test_rampe() {
    ProfileStep step;
    step.type = ProfileStep::RAMPE;
    step.tempStart = 18.0;
    step.tempEnd = 20.0;
    step.durationS = 3600;
    profile.addStep(step);
    profile.start();
    TEST_ASSERT_FLOAT_WITHIN(0.1, 18.0, profile.getCurrentSetpoint());
    mock_advanceMillis(1800000UL); // +30 min -> mi-parcours
    TEST_ASSERT_FLOAT_WITHIN(0.1, 19.0, profile.getCurrentSetpoint());
    mock_advanceMillis(1800000UL); // +30 min -> fin (60 min total)
    TEST_ASSERT_FLOAT_WITHIN(0.1, 20.0, profile.getCurrentSetpoint());
}

// addStep accepte jusqu'à 16 étapes puis refuse ; clearSteps réinitialise
void test_add_clear_steps() {
    ProfileStep step;
    step.type = ProfileStep::PALIER;
    step.tempStart = 18.0;
    step.tempEnd = 18.0;
    step.durationS = 0;
    for (int i = 0; i < 16; i++) {
        TEST_ASSERT_TRUE(profile.addStep(step)); // 16 premières OK
    }
    TEST_ASSERT_FALSE(profile.addStep(step));     // 17e refusée (buffer plein)
    profile.clearSteps();
    TEST_ASSERT_TRUE(profile.addStep(step));       // après clear, à nouveau OK
}

void test_is_active() {
    ProfileStep step;
    step.type = ProfileStep::PALIER;
    step.tempStart = 18.0;
    step.tempEnd = 18.0;
    step.durationS = 0;
    profile.addStep(step);
    profile.start();
    TEST_ASSERT_TRUE(profile.isActive());
    profile.stop();
    TEST_ASSERT_FALSE(profile.isActive());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_palier);
    RUN_TEST(test_rampe);
    RUN_TEST(test_add_clear_steps);
    RUN_TEST(test_is_active);
    return UNITY_END();
}
