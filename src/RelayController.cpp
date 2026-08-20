#include "RelayController.h"

void RelayController::begin() {
    // L'ORDRE EST CRITIQUE.
    // Sur ESP32, pinMode(pin, OUTPUT) force la broche a 0 V. Le module relais
    // etant ACTIF LOW, cela enclencherait le relais pendant l'intervalle entre
    // le pinMode et le premier digitalWrite : c'est le clic entendu a la mise
    // sous tension. On ecrit donc le niveau inactif dans le registre de sortie
    // AVANT de basculer la broche en sortie, puis on le reecrit apres pour
    // confirmation. La broche ne passe ainsi jamais par l'etat actif.

    // Voie FROID (PIN_COOL)
    digitalWrite(PIN_COOL, !COOL_ACTIVE_LEVEL);
    pinMode(PIN_COOL, OUTPUT);
    digitalWrite(PIN_COOL, !COOL_ACTIVE_LEVEL);

    // Voie CHAUD (PIN_HEAT)
    digitalWrite(PIN_HEAT, !HEAT_ACTIVE_LEVEL);
    pinMode(PIN_HEAT, OUTPUT);
    digitalWrite(PIN_HEAT, !HEAT_ACTIVE_LEVEL);

    coolOn = false;
    heatOn = false;

    // Arme la sentinelle de vivacite
    lastKeepAliveMs = millis();
}

void RelayController::setCool(bool on) {
    if (on) {
        applyOutputs(true, false);      // exclusivite : chaud coupe
    } else {
        applyOutputs(false, heatOn);    // chaud inchange
    }
}

void RelayController::setHeat(bool on) {
    if (on) {
        applyOutputs(false, true);      // exclusivite : froid coupe
    } else {
        applyOutputs(coolOn, false);    // froid inchange
    }
}

void RelayController::allOff() {
    applyOutputs(false, false);
}

bool RelayController::isCoolOn() const {
    return coolOn;
}

bool RelayController::isHeatOn() const {
    return heatOn;
}

void RelayController::keepAlive() {
    lastKeepAliveMs = millis();
}

bool RelayController::checkKeepAlive() {
    // Comparaison anti-debordement : toujours millis() - horodatage
    if ((millis() - lastKeepAliveMs) >= ((unsigned long)RELAY_KEEPALIVE_TIMEOUT_S * 1000UL)) {
        if (coolOn || heatOn) {
            applyOutputs(false, false);
            Serial.println("[RELAY] sentinelle de vivacite expiree - sorties coupees");
            return true;
        }
    }
    return false;
}

void RelayController::applyOutputs(bool cool, bool heat) {
    // Exclusivite structurelle : une demande simultanee des deux voies est un
    // defaut logique de l'appelant. On coupe TOUT et on le signale.
    if (cool && heat) {
        digitalWrite(PIN_COOL, !COOL_ACTIVE_LEVEL);
        digitalWrite(PIN_HEAT, !HEAT_ACTIVE_LEVEL);
        coolOn = false;
        heatOn = false;
        Serial.println("[RELAY] ERREUR : les deux voies demandees en meme temps - coupure des deux");
        return;
    }

    // 1) Coupures d'abord, et INCONDITIONNELLEMENT : une coupure ne doit
    //    jamais etre sautee au motif que l'etat bufferise la croit deja faite.
    if (!cool) {
        digitalWrite(PIN_COOL, !COOL_ACTIVE_LEVEL);
    }
    if (!heat) {
        digitalWrite(PIN_HEAT, !HEAT_ACTIVE_LEVEL);
    }

    // 2) Activations ensuite, et seulement si la voie n'est pas deja active :
    //    inutile de repulser un relais deja enclenche.
    if (cool && !coolOn) {
        digitalWrite(PIN_COOL, COOL_ACTIVE_LEVEL);
    }
    if (heat && !heatOn) {
        digitalWrite(PIN_HEAT, HEAT_ACTIVE_LEVEL);
    }

    coolOn = cool;
    heatOn = heat;
}
