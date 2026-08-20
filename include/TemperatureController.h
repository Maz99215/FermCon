#ifndef TEMPERATURE_CONTROLLER_H
#define TEMPERATURE_CONTROLLER_H

#include "Config.h"
#include "RelayController.h"
#include <math.h>
#include <stdint.h>

class TemperatureController {
public:
    enum class State { IDLE, COOLING, HEATING, FAULT };

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

    // Accesseurs ajoutes pour exposition dans l'API HTTP
    uint32_t getFaultCount() const;        // nombre cumule d'entrees en defaut depuis le demarrage
    uint32_t getLastFaultEpoch() const;    // epoch UNIX du dernier defaut, 0 si inconnu
    float getLastRejectedReading() const;  // derniere valeur brute jugee non plausible
    bool isFaultPending() const;           // lectures invalides en cours d'accumulation
    bool hasValidReading() const;          // au moins une mesure plausible obtenue

private:
    RelayController* _relays;
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

    // Verrou de premiere mesure : aucune regulation avant une mesure plausible.
    // Sans ce verrou, _currentTemp vaut 0.0 au demarrage et la voie CHAUD
    // s'enclencherait avant la premiere lecture de sonde.
    bool _hasValidReading;

    bool isReadingPlausible(float t) const;
    void readTemperature();
    void controlTemperature();
    void checkTimeouts();
};

#endif // TEMPERATURE_CONTROLLER_H
