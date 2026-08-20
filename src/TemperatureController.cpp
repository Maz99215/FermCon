#include "TemperatureController.h"
#include "Config.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <time.h>

#ifndef NTP_VALID_EPOCH_MIN
#define NTP_VALID_EPOCH_MIN 1600000000UL
#endif

OneWire oneWire(PIN_DS18B20);
DallasTemperature sensors(&oneWire);

TemperatureController::TemperatureController()
    : _relays(nullptr), _setpoint(0.0f), _currentTemp(0.0f),
      _state(State::IDLE), _fault(false),
      _lastCoolOnTime(0), _lastCoolOffTime(0), _lastHeatOnTime(0), _lastHeatOffTime(0),
      _coolOnTime(0), _heatOnTime(0),
      _lastReadTime(0),
      _invalidSince(0), _validSince(0), _faultPending(false),
      _faultCount(0), _lastFaultEpoch(0), _lastRejectedReading(0.0f),
      _hasValidReading(false) {}

void TemperatureController::begin(RelayController* relays) {
    _relays = relays;
    _state = State::IDLE;
    _fault = false;
    _faultPending = false;
    _coolOnTime = 0;
    _heatOnTime = 0;
    _lastCoolOnTime = 0;
    _lastHeatOnTime = 0;
    _lastReadTime = 0;
    _invalidSince = 0;
    _validSince = 0;
    _faultCount = 0;
    _lastFaultEpoch = 0;
    _lastRejectedReading = 0.0f;
    _hasValidReading = false;

    // Anti-court-cycle ARME au demarrage, contrairement au comportement
    // precedent qui antidatait les extinctions. Le delai minimal d'arret du
    // compresseur (COMPRESSOR_MIN_OFF_S) s'applique donc pleinement apres une
    // mise sous tension, un retour de coupure secteur ou un rechargement du
    // firmware. Choix volontaire : proteger le compresseur du frigo.
    _lastCoolOffTime = millis();
    _lastHeatOffTime = millis();

    sensors.begin();
}

void TemperatureController::update() {
    if (!_relays) return;

    unsigned long currentTime = millis();
    if (currentTime - _lastReadTime >= TEMP_READ_INTERVAL_MS) {
        readTemperature();
        _lastReadTime = currentTime;
    }
    controlTemperature();
    checkTimeouts();

    // Sentinelle de vivacite : appelee inconditionnellement apres chaque cycle
    // de regulation. Un passage en defaut compte comme un cycle abouti.
    _relays->keepAlive();
}

bool TemperatureController::isReadingPlausible(float t) const {
    if (isnan(t)) return false;
    if (fabsf(t - (float)DEVICE_DISCONNECTED_C) < 0.1f) return false;
    if (fabsf(t - DS18B20_ERR_LOW) < 0.1f) return false;
    if (fabsf(t - DS18B20_ERR_HIGH) < 0.1f) return false;
    if (t < TEMP_PLAUSIBLE_MIN_C || t > TEMP_PLAUSIBLE_MAX_C) return false;
    return true;
}

void TemperatureController::readTemperature() {
    sensors.requestTemperatures();
    float rawTemp = sensors.getTempCByIndex(0);

    if (!isReadingPlausible(rawTemp)) {
        _lastRejectedReading = rawTemp;

        if (!_fault) {
            // Accumulation de la duree continue de lectures invalides
            if (!_faultPending) {
                _invalidSince = millis();
                _faultPending = true;
            }
            if (millis() - _invalidSince >= (unsigned long)TEMP_FAULT_TRIP_S * 1000UL) {
                _fault = true;
                _state = State::FAULT;
                _relays->allOff();
                _faultCount++;

                time_t now = time(nullptr);
                if (now > (time_t)NTP_VALID_EPOCH_MIN) {
                    _lastFaultEpoch = (uint32_t)now;
                }

                Serial.print("[TEMP] DEFAUT declare - derniere valeur rejetee=");
                Serial.println(rawTemp);
            }
        } else {
            // En defaut : toute lecture invalide annule la reprise en cours
            _validSince = 0;
        }
    } else {
        // Lecture plausible : elle devient la temperature courante
        _currentTemp = rawTemp;
        _hasValidReading = true;

        if (!_fault) {
            _faultPending = false;
            _invalidSince = 0;
        } else {
            // Accumulation de la duree continue de lectures plausibles
            if (_validSince == 0) {
                _validSince = millis();
            }
            if (millis() - _validSince >= (unsigned long)TEMP_FAULT_CLEAR_S * 1000UL) {
                _fault = false;
                _state = State::IDLE;
                _faultPending = false;
                _invalidSince = 0;
                _validSince = 0;
                Serial.println("[TEMP] DEFAUT leve - sonde a nouveau plausible");
            }
        }
    }
}

void TemperatureController::controlTemperature() {
    if (!_relays) return;

    if (_fault) {
        _relays->allOff();
        _state = State::FAULT;
        return;
    }

    // Aucune regulation avant la premiere mesure plausible : _currentTemp vaut
    // 0.0 au demarrage, ce qui declencherait une demande de chaud immediate.
    if (!_hasValidReading) {
        _relays->allOff();
        _state = State::IDLE;
        return;
    }

    unsigned long currentTime = millis();

    if (_currentTemp > _setpoint + TEMP_HYSTERESIS_C) {
        // Demande de froid : couper le chaud immediatement
        // (charge resistive, aucune contrainte de cycle)
        if (_relays->isHeatOn()) {
            _relays->setHeat(false);
            _lastHeatOffTime = currentTime;
            Serial.println("[TEMP] Chaud coupe (demande de froid)");
        }

        if (!_relays->isCoolOn()) {
            if (currentTime - _lastCoolOffTime >= (unsigned long)COMPRESSOR_MIN_OFF_S * 1000UL) {
                _relays->setCool(true);
                _state = State::COOLING;
                _lastCoolOnTime = currentTime;
                _coolOnTime = currentTime;
            } else {
                // Attente du delai anti-court-cycle compresseur :
                // aucune sortie active, etat IDLE
                _state = State::IDLE;
            }
        }
    } else if (_currentTemp < _setpoint - TEMP_HYSTERESIS_C) {
        // Demande de chaud : couper le froid en respectant COOL_MIN_ON_S
        // (protection du compresseur contre les redemarrages trop rapproches)
        if (_relays->isCoolOn()) {
            if (currentTime - _coolOnTime >= (unsigned long)COOL_MIN_ON_S * 1000UL) {
                _relays->setCool(false);
                _lastCoolOffTime = currentTime;
                Serial.println("[TEMP] Froid coupe (demande de chaud)");
            }
        }

        // N'activer le chaud que si le froid est bien coupe
        // (sinon applyOutputs() contournerait silencieusement COOL_MIN_ON_S)
        if (!_relays->isCoolOn() && !_relays->isHeatOn()) {
            _relays->setHeat(true);
            _state = State::HEATING;
            _lastHeatOnTime = currentTime;
            _heatOnTime = currentTime;
        }
    } else {
        // Dans la bande d'hysteresis : extinction apres la duree minimale d'activation
        if (_relays->isCoolOn()) {
            if (currentTime - _coolOnTime >= (unsigned long)COOL_MIN_ON_S * 1000UL) {
                _relays->setCool(false);
                _lastCoolOffTime = currentTime;
                _state = State::IDLE;
            }
        } else if (_relays->isHeatOn()) {
            if (currentTime - _heatOnTime >= (unsigned long)HEAT_MIN_ON_S * 1000UL) {
                _relays->setHeat(false);
                _lastHeatOffTime = currentTime;
                _state = State::IDLE;
            }
        } else {
            _state = State::IDLE;
        }
    }
}

void TemperatureController::checkTimeouts() {
    if (!_relays) return;

    unsigned long currentTime = millis();
    if (_relays->isCoolOn() && (currentTime - _coolOnTime >= (unsigned long)MAX_ON_TIMEOUT_S * 1000UL)) {
        _relays->setCool(false);
        _lastCoolOffTime = currentTime;
        _state = State::IDLE;
        Serial.println("[TEMP] timeout d'activation FROID - sortie coupee");
    }
    if (_relays->isHeatOn() && (currentTime - _heatOnTime >= (unsigned long)MAX_ON_TIMEOUT_S * 1000UL)) {
        _relays->setHeat(false);
        _lastHeatOffTime = currentTime;
        _state = State::IDLE;
        Serial.println("[TEMP] timeout d'activation CHAUD - sortie coupee");
    }
}

void TemperatureController::setSetpoint(float setpoint) { _setpoint = setpoint; }
float TemperatureController::getCurrentTemp() const { return _currentTemp; }
float TemperatureController::getSetpoint() const { return _setpoint; }
TemperatureController::State TemperatureController::getState() const { return _state; }
bool TemperatureController::isFault() const { return _fault; }
bool TemperatureController::isCoolOn() const { return _relays ? _relays->isCoolOn() : false; }
bool TemperatureController::isHeatOn() const { return _relays ? _relays->isHeatOn() : false; }

uint32_t TemperatureController::getFaultCount() const { return _faultCount; }
uint32_t TemperatureController::getLastFaultEpoch() const { return _lastFaultEpoch; }
float TemperatureController::getLastRejectedReading() const { return _lastRejectedReading; }
bool TemperatureController::isFaultPending() const { return _faultPending && !_fault; }
bool TemperatureController::hasValidReading() const { return _hasValidReading; }
