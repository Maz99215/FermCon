#ifndef TEMPERATURE_CONTROLLER_H
#define TEMPERATURE_CONTROLLER_H

#include "Config.h"
#include "RelayController.h"

class TemperatureController {
public:
    enum class State {
        IDLE,
        COOLING,
        HEATING,
        FAULT
    };

    TemperatureController();
    void begin(RelayController* relays);
    void update();
    void setSetpoint(float setpoint);
    float getCurrentTemp() const;
    float getSetpoint() const;
    State getState() const;
    bool isFault() const;
    bool isCoolOn() const;
    bool isHeatOn() const;

#ifdef UNIT_TEST
    // Injection de température pour tests natifs (pas d'accès capteur)
    void setCurrentTempForTest(float t);
#endif

private:
    RelayController* _relays;
    float _setpoint;
    float _currentTemp;
    State _state;
    bool _fault;

    // Horodatages anti-court-cycle
    unsigned long _lastCoolOnTime;
    unsigned long _lastCoolOffTime;
    unsigned long _lastHeatOnTime;
    unsigned long _lastHeatOffTime;

    // Suivi des timeouts de sécurité
    unsigned long _coolOnTime;
    unsigned long _heatOnTime;

    void readTemperature();
    void controlTemperature();
    void checkTimeouts();
};

#endif // TEMPERATURE_CONTROLLER_H
