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
    : _relays(nullptr), _config(nullptr), _setpoint(0.0f), _currentTemp(0.0f),
      _state(State::IDLE), _fault(false),
      _lastCoolOnTime(0), _lastCoolOffTime(0), _lastHeatOnTime(0), _lastHeatOffTime(0),
      _coolOnTime(0), _heatOnTime(0),
      _lastReadTime(0),
      _invalidSince(0), _validSince(0), _faultPending(false),
      _faultCount(0), _lastFaultEpoch(0), _lastRejectedReading(0.0f),
      _hasValidReading(false) {}

void TemperatureController::begin(RelayController* relays, const SystemConfig* config) {
    _relays = relays;
    _config = config;
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

    _lastCoolOffTime = millis();
    _lastHeatOffTime = millis();

    sensors.begin();
}

void TemperatureController::update() {
    if (!_relays || !_config) return;

    unsigned long currentTime = millis();
    // Lire la periode d'echantillonnage depuis SystemConfig
    uint32_t readInterval = _config->temp_read_interval_ms;
    if (currentTime - _lastReadTime >= readInterval) {
        readTemperature();
        _lastReadTime = currentTime;
    }
    controlTemperature();
    checkTimeouts();

    _relays->keepAlive();
}

bool TemperatureController::isReadingPlausible(float t) const {
    if (isnan(t)) return false;
    if (fabsf(t - (float)DEVICE_DISCONNECTED_C) < 0.1f) return false;
    if (fabsf(t - DS18B20_ERR_LOW) < 0.1f) return false;
    if (fabsf(t - DS18B20_ERR_HIGH) < 0.1f) return false;
    // Lire les plages de plausibilite depuis SystemConfig
    float plausMin = _config->temp_plausible_min_c;
    float plausMax = _config->temp_plausible_max_c;
    if (t < plausMin || t > plausMax) return false;
    return true;
}

void TemperatureController::readTemperature() {
    sensors.requestTemperatures();
    float rawTemp = sensors.getTempCByIndex(0);

    if (!isReadingPlausible(rawTemp)) {
        _lastRejectedReading = rawTemp;

        // Lire temporisations de defaut depuis SystemConfig
        uint32_t tripS = _config->temp_fault_trip_s;
        uint32_t clearS = _config->temp_fault_clear_s;

        if (!_fault) {
            if (!_faultPending) {
                _invalidSince = millis();
                _faultPending = true;
            }
            if (millis() - _invalidSince >= (unsigned long)tripS * 1000UL) {
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
            _validSince = 0;
        }
    } else {
        // Lecture plausible : appliquer l'offset de sonde depuis SystemConfig
        _currentTemp = rawTemp + _config->temp_offset;
        _hasValidReading = true;

        // Lire temporisations de defaut depuis SystemConfig
        uint32_t clearS = _config->temp_fault_clear_s;

        if (!_fault) {
            _faultPending = false;
            _invalidSince = 0;
        } else {
            if (_validSince == 0) {
                _validSince = millis();
            }
            if (millis() - _validSince >= (unsigned long)clearS * 1000UL) {
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
    if (!_relays || !_config) return;

    if (_fault) {
        _relays->allOff();
        _state = State::FAULT;
        return;
    }

    if (!_hasValidReading) {
        _relays->allOff();
        _state = State::IDLE;
        return;
    }

    unsigned long currentTime = millis();

    // Lire hysteresis depuis SystemConfig
    float hysteresis = _config->hysteresis;

    if (_currentTemp > _setpoint + hysteresis) {
        if (_relays->isHeatOn()) {
            _relays->setHeat(false);
            _lastHeatOffTime = currentTime;
            Serial.println("[TEMP] Chaud coupe (demande de froid)");
        }

        if (!_relays->isCoolOn()) {
            // Lire anti-court-cycle depuis SystemConfig
            uint32_t compressorDelay = _config->min_compressor_delay;
            if (currentTime - _lastCoolOffTime >= (unsigned long)compressorDelay * 1000UL) {
                _relays->setCool(true);
                _state = State::COOLING;
                _lastCoolOnTime = currentTime;
                _coolOnTime = currentTime;
            } else {
                _state = State::IDLE;
            }
        }
    } else if (_currentTemp < _setpoint - hysteresis) {
        // Lire duree minimale froid depuis SystemConfig
        uint32_t coolMinOn = _config->cool_min_on_s;
        if (_relays->isCoolOn()) {
            if (currentTime - _coolOnTime >= (unsigned long)coolMinOn * 1000UL) {
                _relays->setCool(false);
                _lastCoolOffTime = currentTime;
                Serial.println("[TEMP] Froid coupe (demande de chaud)");
            }
        }

        if (!_relays->isCoolOn() && !_relays->isHeatOn()) {
            _relays->setHeat(true);
            _state = State::HEATING;
            _lastHeatOnTime = currentTime;
            _heatOnTime = currentTime;
        }
    } else {
        // Dans la bande d'hysteresis
        uint32_t coolMinOn = _config->cool_min_on_s;
        uint32_t heatMinOn = _config->heat_min_on_s;
        if (_relays->isCoolOn()) {
            if (currentTime - _coolOnTime >= (unsigned long)coolMinOn * 1000UL) {
                _relays->setCool(false);
                _lastCoolOffTime = currentTime;
                _state = State::IDLE;
            }
        } else if (_relays->isHeatOn()) {
            if (currentTime - _heatOnTime >= (unsigned long)heatMinOn * 1000UL) {
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
    if (!_relays || !_config) return;

    unsigned long currentTime = millis();
    // Lire timeout max depuis SystemConfig
    uint32_t maxTimeout = _config->max_on_timeout_s;

    if (_relays->isCoolOn() && _coolOnTime > 0) {
        if (currentTime - _coolOnTime >= (unsigned long)maxTimeout * 1000UL) {
            _relays->setCool(false);
            _lastCoolOffTime = currentTime;
            _state = State::IDLE;
            Serial.println("[TEMP] TIMEOUT securite froid");
        }
    }
    if (_relays->isHeatOn() && _heatOnTime > 0) {
        if (currentTime - _heatOnTime >= (unsigned long)maxTimeout * 1000UL) {
            _relays->setHeat(false);
            _lastHeatOffTime = currentTime;
            _state = State::IDLE;
            Serial.println("[TEMP] TIMEOUT securite chaud");
        }
    }
}

float TemperatureController::getCurrentTemp() const { return _currentTemp; }
float TemperatureController::getSetpoint() const { return _setpoint; }
void TemperatureController::setSetpoint(float setpoint) { _setpoint = setpoint; }

TemperatureController::State TemperatureController::getState() const { return _state; }
bool TemperatureController::isFault() const { return _fault; }
bool TemperatureController::isCoolOn() const { return _relays ? _relays->isCoolOn() : false; }
bool TemperatureController::isHeatOn() const { return _relays ? _relays->isHeatOn() : false; }

uint32_t TemperatureController::getFaultCount() const { return _faultCount; }
uint32_t TemperatureController::getLastFaultEpoch() const { return _lastFaultEpoch; }
float TemperatureController::getLastRejectedReading() const { return _lastRejectedReading; }
bool TemperatureController::isFaultPending() const { return _faultPending; }
bool TemperatureController::hasValidReading() const { return _hasValidReading; }
