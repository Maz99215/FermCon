#include "RelayController.h"

void RelayController::begin() {
    // Configuration des pins en sortie
    pinMode(PIN_COOL, OUTPUT);
    pinMode(PIN_HEAT, OUTPUT);

    // Initialisation des sorties à OFF sûr
    digitalWrite(PIN_COOL, !COOL_ACTIVE_LEVEL);
    digitalWrite(PIN_HEAT, !HEAT_ACTIVE_LEVEL);
}

void RelayController::setCool(bool on) {
    if (on) {
        // Si on active FROID, on désactive CHAUD d'abord
        digitalWrite(PIN_HEAT, !HEAT_ACTIVE_LEVEL);
        heatOn = false;
    }
    digitalWrite(PIN_COOL, on ? COOL_ACTIVE_LEVEL : !COOL_ACTIVE_LEVEL);
    coolOn = on;
}

void RelayController::setHeat(bool on) {
    if (on) {
        // Si on active CHAUD, on désactive FROID d'abord
        digitalWrite(PIN_COOL, !COOL_ACTIVE_LEVEL);
        coolOn = false;
    }
    digitalWrite(PIN_HEAT, on ? HEAT_ACTIVE_LEVEL : !HEAT_ACTIVE_LEVEL);
    heatOn = on;
}

void RelayController::allOff() {
    digitalWrite(PIN_COOL, !COOL_ACTIVE_LEVEL);
    digitalWrite(PIN_HEAT, !HEAT_ACTIVE_LEVEL);
    coolOn = false;
    heatOn = false;
}

bool RelayController::isCoolOn() const {
    return coolOn;
}

bool RelayController::isHeatOn() const {
    return heatOn;
}