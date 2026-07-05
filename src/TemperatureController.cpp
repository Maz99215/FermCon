#include "TemperatureController.h"
#include "Config.h"

#ifndef UNIT_TEST
#include <OneWire.h>
#include <DallasTemperature.h>

// Capteur DS18B20 (uniquement en build matériel)
OneWire oneWire(PIN_DS18B20);
DallasTemperature sensors(&oneWire);
#endif

TemperatureController::TemperatureController()
    : _relays(nullptr),
      _setpoint(0.0f),
      _currentTemp(0.0f),
      _state(State::IDLE),
      _fault(false),
      _lastCoolOnTime(0),
      _lastCoolOffTime(0),
      _lastHeatOnTime(0),
      _lastHeatOffTime(0),
      _coolOnTime(0),
      _heatOnTime(0) {}

void TemperatureController::begin(RelayController* relays) {
    _relays = relays;

    // Repli à un état propre (utile au boot et entre deux tests)
    _state = State::IDLE;
    _fault = false;
    _coolOnTime = 0;
    _heatOnTime = 0;
    _lastCoolOnTime = 0;
    _lastHeatOnTime = 0;

    // IMPORTANT : on antidate les dernières extinctions pour que le premier
    // démarrage ne soit pas bloqué par l'anti-court-cycle (sinon, au boot,
    // now - _lastCoolOffTime < COMPRESSOR_MIN_OFF_S et le froid ne démarrerait
    // jamais pendant les 5 premières minutes).
    unsigned long now = millis();
    _lastCoolOffTime = now - (unsigned long)COMPRESSOR_MIN_OFF_S * 1000UL;
    _lastHeatOffTime = now - (unsigned long)HEAT_MIN_ON_S * 1000UL;

#ifndef UNIT_TEST
    sensors.begin();
#endif
}

void TemperatureController::update() {
#ifndef UNIT_TEST
    // Build matériel : lecture périodique du capteur.
    static unsigned long lastReadTime = 0;
    unsigned long currentTime = millis();
    if (currentTime - lastReadTime >= TEMP_READ_INTERVAL_MS) {
        readTemperature();
        lastReadTime = currentTime;
    }
#endif
    // En build UNIT_TEST, _currentTemp/_fault sont fournis par setCurrentTempForTest().
    controlTemperature();
    checkTimeouts();
}

void TemperatureController::readTemperature() {
#ifndef UNIT_TEST
    sensors.requestTemperatures();
    _currentTemp = sensors.getTempCByIndex(0);

    // Détection d'erreur sonde -> repli sûr
    if (_currentTemp == DS18B20_ERR_LOW || _currentTemp == DS18B20_ERR_HIGH) {
        _fault = true;
        _relays->allOff();
        _state = State::FAULT;
    } else {
        _fault = false;
    }
#endif
}

void TemperatureController::controlTemperature() {
    // Repli sûr : en défaut, tout est coupé.
    if (_fault) {
        _relays->allOff();
        _state = State::FAULT;
        return;
    }

    unsigned long currentTime = millis();

    if (_currentTemp > _setpoint + TEMP_HYSTERESIS_C) {
        // Demande de FROID
        if (!_relays->isCoolOn()) {
            // Anti-court-cycle compresseur : respecter le temps d'arrêt minimal.
            if (currentTime - _lastCoolOffTime >= (unsigned long)COMPRESSOR_MIN_OFF_S * 1000UL) {
                _relays->setCool(true);   // exclusivité gérée par RelayController
                _relays->setHeat(false);
                _state = State::COOLING;
                _lastCoolOnTime = currentTime;
                _coolOnTime = currentTime;
            }
            // sinon : on attend la fin du temps d'arrêt minimal (reste OFF)
        }
    } else if (_currentTemp < _setpoint - TEMP_HYSTERESIS_C) {
        // Demande de CHAUD
        if (!_relays->isHeatOn()) {
            _relays->setHeat(true);
            _relays->setCool(false);
            _state = State::HEATING;
            _lastHeatOnTime = currentTime;
            _heatOnTime = currentTime;
        }
    } else {
        // Zone morte (hystérésis) : on coupe, en respectant la marche minimale.
        if (_relays->isCoolOn()) {
            if (currentTime - _coolOnTime >= (unsigned long)COOL_MIN_ON_S * 1000UL) {
                _relays->setCool(false);
                _lastCoolOffTime = currentTime;
                _state = State::IDLE;
            }
            // sinon : marche minimale non atteinte, le froid reste ON
        } else if (_relays->isHeatOn()) {
            if (currentTime - _heatOnTime >= (unsigned long)HEAT_MIN_ON_S * 1000UL) {
                _relays->setHeat(false);
                _lastHeatOffTime = currentTime;
                _state = State::IDLE;
            }
            // sinon : marche minimale non atteinte, le chaud reste ON
        } else {
            _state = State::IDLE;
        }
    }
}

void TemperatureController::checkTimeouts() {
    unsigned long currentTime = millis();

    // Sécurité : coupe une sortie restée active anormalement longtemps.
    if (_relays->isCoolOn() && (currentTime - _coolOnTime >= (unsigned long)MAX_ON_TIMEOUT_S * 1000UL)) {
        _relays->setCool(false);
        _lastCoolOffTime = currentTime;
        _state = State::IDLE;
    }
    if (_relays->isHeatOn() && (currentTime - _heatOnTime >= (unsigned long)MAX_ON_TIMEOUT_S * 1000UL)) {
        _relays->setHeat(false);
        _lastHeatOffTime = currentTime;
        _state = State::IDLE;
    }
}

void TemperatureController::setSetpoint(float setpoint) {
    _setpoint = setpoint;
}

float TemperatureController::getCurrentTemp() const {
    return _currentTemp;
}

float TemperatureController::getSetpoint() const {
    return _setpoint;
}

TemperatureController::State TemperatureController::getState() const {
    return _state;
}

bool TemperatureController::isFault() const {
    return _fault;
}

bool TemperatureController::isCoolOn() const {
    return _relays->isCoolOn();
}

bool TemperatureController::isHeatOn() const {
    return _relays->isHeatOn();
}

#ifdef UNIT_TEST
void TemperatureController::setCurrentTempForTest(float t) {
    _currentTemp = t;
    _fault = (t == DS18B20_ERR_LOW || t == DS18B20_ERR_HIGH);
}
#endif
