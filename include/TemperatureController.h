#ifndef TEMPERATURE_CONTROLLER_H
#define TEMPERATURE_CONTROLLER_H

#include "Config.h"
#include "ConfigStore.h"
#include "RelayController.h"
#include <math.h>
#include <stdint.h>

class TemperatureController {
public:
    enum class State { IDLE, COOLING, HEATING, FAULT };

    TemperatureController();
    void begin(RelayController* relays, const SystemConfig* config);
    void update();
    void setSetpoint(float setpoint);
    float getCurrentTemp() const;
    float getSetpoint() const;
    State getState() const;
    bool isFault() const;
    bool isCoolOn() const;
    bool isHeatOn() const;

    // Accesseurs ajoutes pour exposition dans l'API HTTP
    uint32_t getFaultCount() const;
    uint32_t getLastFaultEpoch() const;
    float getLastRejectedReading() const;
    bool isFaultPending() const;
    bool hasValidReading() const;

private:
    RelayController* _relays;
    const SystemConfig* _config;   // NOUVEAU v0.3.0 : pointeur constant vers la config en RAM
    float _setpoint;
    float _currentTemp;
    State _state;
    bool _fault;

    // Horodatages anti-court-cycle (millis)
    unsigned long _lastCoolOnTime;
    unsigned long _lastCoolOffTime;
    unsigned long _lastHeatOnTime;
    unsigned long _lastHeatOffTime;

    // Suivi des timeouts de securite (millis)
    unsigned long _coolOnTime;
    unsigned long _heatOnTime;

    // Cadencement des lectures de sonde
    unsigned long _lastReadTime;

    // Chronometres de defaut (millis)
    unsigned long _invalidSince;
    unsigned long _validSince;
    bool _faultPending;

    // Statistiques de defaut
    uint32_t _faultCount;
    uint32_t _lastFaultEpoch;
    float _lastRejectedReading;

    // Verrou de premiere mesure
    bool _hasValidReading;

    bool isReadingPlausible(float t) const;
    void readTemperature();
    void controlTemperature();
    void checkTimeouts();
};

#endif // TEMPERATURE_CONTROLLER_H
