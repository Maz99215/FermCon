#include <Arduino.h>
#include <RelayController.h>
#include <TemperatureController.h>
#include <unity.h>

// State est un type imbriqué : TemperatureController::State
using State = TemperatureController::State;

RelayController relays;
TemperatureController tc;   // constructeur sans argument ; begin() relie le relais

void setUp(void) {
    mock_resetMillis();
    relays.begin();
    tc.begin(&relays);
    tc.setSetpoint(18.0);
}

void tearDown(void) {
    mock_resetMillis();
}

// Demande de froid : temp > consigne + hystérésis
void test_demande_froid(void) {
    tc.setCurrentTempForTest(20.0);
    tc.update();
    TEST_ASSERT_EQUAL(State::COOLING, tc.getState());
    TEST_ASSERT_TRUE(tc.isCoolOn());
    TEST_ASSERT_FALSE(tc.isHeatOn());
}

// Demande de chaud : temp < consigne - hystérésis
void test_demande_chaud(void) {
    tc.setCurrentTempForTest(16.0);
    tc.update();
    TEST_ASSERT_EQUAL(State::HEATING, tc.getState());
    TEST_ASSERT_TRUE(tc.isHeatOn());
    TEST_ASSERT_FALSE(tc.isCoolOn());
}

// Zone morte : dans la bande d'hystérésis -> repos
void test_zone_morte(void) {
    tc.setCurrentTempForTest(18.0);
    tc.update();
    TEST_ASSERT_EQUAL(State::IDLE, tc.getState());
    TEST_ASSERT_FALSE(tc.isCoolOn());
    TEST_ASSERT_FALSE(tc.isHeatOn());
}

// Jamais FROID et CHAUD simultanés
void test_exclusivite(void) {
    tc.setCurrentTempForTest(20.0);
    tc.update();
    TEST_ASSERT_TRUE(tc.isCoolOn());
    // Bascule directe vers une demande de chaud
    tc.setCurrentTempForTest(16.0);
    tc.update();
    TEST_ASSERT_FALSE(tc.isCoolOn());
    TEST_ASSERT_TRUE(tc.isHeatOn());
}

// Marche minimale du froid : ne pas couper avant COOL_MIN_ON_S (120 s)
void test_min_on_froid(void) {
    tc.setCurrentTempForTest(20.0);
    tc.update();                       // t=0 : froid ON
    TEST_ASSERT_TRUE(tc.isCoolOn());

    tc.setCurrentTempForTest(18.0);    // retour en zone morte
    mock_advanceMillis(119UL * 1000UL);
    tc.update();                       // 119 s < 120 s -> reste ON
    TEST_ASSERT_TRUE(tc.isCoolOn());

    mock_advanceMillis(1UL * 1000UL);
    tc.update();                       // 120 s atteints -> OFF
    TEST_ASSERT_FALSE(tc.isCoolOn());
}

// Anti-court-cycle compresseur : pas de redémarrage avant COMPRESSOR_MIN_OFF_S (300 s)
void test_anti_court_cycle_froid(void) {
    tc.setCurrentTempForTest(20.0);
    tc.update();                       // t=0 : froid ON
    TEST_ASSERT_TRUE(tc.isCoolOn());

    // Repasser en zone morte ET dépasser la marche minimale pour éteindre
    tc.setCurrentTempForTest(18.0);
    mock_advanceMillis(120UL * 1000UL);
    tc.update();                       // t=120 s : froid OFF (lastCoolOff=120 s)
    TEST_ASSERT_FALSE(tc.isCoolOn());

    // Nouvelle demande de froid mais < 300 s depuis l'extinction -> bloqué
    tc.setCurrentTempForTest(20.0);
    mock_advanceMillis(299UL * 1000UL);
    tc.update();                       // 299 s d'arrêt -> pas de redémarrage
    TEST_ASSERT_FALSE(tc.isCoolOn());

    mock_advanceMillis(2UL * 1000UL);
    tc.update();                       // 301 s d'arrêt -> redémarrage autorisé
    TEST_ASSERT_TRUE(tc.isCoolOn());
}

// Repli sûr sur erreur sonde puis reprise
void test_repli_faute(void) {
    tc.setCurrentTempForTest(-127.0);  // DS18B20_ERR_LOW
    tc.update();
    TEST_ASSERT_EQUAL(State::FAULT, tc.getState());
    TEST_ASSERT_TRUE(tc.isFault());
    TEST_ASSERT_FALSE(tc.isCoolOn());
    TEST_ASSERT_FALSE(tc.isHeatOn());

    tc.setCurrentTempForTest(18.0);    // lecture redevenue valide
    tc.update();
    TEST_ASSERT_EQUAL(State::IDLE, tc.getState());
    TEST_ASSERT_FALSE(tc.isFault());
}

// Timeout de sécurité : coupe une sortie restée active trop longtemps
void test_timeout_securite(void) {
    tc.setCurrentTempForTest(20.0);
    tc.update();                       // froid ON
    TEST_ASSERT_TRUE(tc.isCoolOn());

    mock_advanceMillis(7200UL * 1000UL); // MAX_ON_TIMEOUT_S
    tc.update();
    TEST_ASSERT_FALSE(tc.isCoolOn());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_demande_froid);
    RUN_TEST(test_demande_chaud);
    RUN_TEST(test_zone_morte);
    RUN_TEST(test_exclusivite);
    RUN_TEST(test_min_on_froid);
    RUN_TEST(test_anti_court_cycle_froid);
    RUN_TEST(test_repli_faute);
    RUN_TEST(test_timeout_securite);
    return UNITY_END();
}
