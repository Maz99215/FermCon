#include <unity.h>
#include "Arduino.h"
#include "time_mock.h"

// Variables globales pour les tests
static unsigned long initial_millis;

// Fonction de configuration avant chaque test
void setUp(void) {
    // Initialisation avant chaque test
    mock_resetMillis();
    initial_millis = millis();
}

// Fonction de nettoyage après chaque test
void tearDown(void) {
    // Nettoyage après chaque test
}

// Test de l'horloge mock
void test_millis(void) {
    TEST_ASSERT_EQUAL_UINT32(0, millis());

    mock_setMillis(1000);
    TEST_ASSERT_EQUAL_UINT32(1000, millis());

    mock_advanceMillis(500);
    TEST_ASSERT_EQUAL_UINT32(1500, millis());
}

// Test du formatage String(float, decimals)
void test_string_formatting(void) {
    String str1(3.14159, 2);
    TEST_ASSERT_EQUAL_STRING("3.14", str1.c_str());

    String str2(2.71828, 3);
    TEST_ASSERT_EQUAL_STRING("2.718", str2.c_str());
}

// Fonction principale pour exécuter les tests
int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_millis);
    RUN_TEST(test_string_formatting);

    return UNITY_END();
}