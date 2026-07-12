#ifndef RELAY_CONTROLLER_H
#define RELAY_CONTROLLER_H

#include "Config.h"
#include <Arduino.h>

/**
 * Contrôleur de relai pour le système de fermentation FermCon.
 * Gère deux sorties indépendantes :
 * - FROID : SSR actif au niveau HIGH
 * - CHAUD : Relais mécanique actif au niveau LOW
 */

class RelayController {
public:
    /**
     * Initialise les pins de contrôle.
     * Les deux sorties sont immédiatement mises à OFF sûr.
     */
    void begin();

    /**
     * Active ou désactive le contrôle de la sortie FROID.
     * @param on true pour activer, false pour désactiver.
     * Si on==true, force la sortie CHAUD à OFF d'abord (exclusivité).
     */
    void setCool(bool on);

    /**
     * Active ou désactive le contrôle de la sortie CHAUD.
     * @param on true pour activer, false pour désactiver.
     * Si on==true, force la sortie FROID à OFF d'abord (exclusivité).
     */
    void setHeat(bool on);

    /**
     * Désactive toutes les sorties (repli sûr).
     */
    void allOff();

    /**
     * @return true si la sortie FROID est active.
     */
    bool isCoolOn() const;

    /**
     * @return true si la sortie CHAUD est active.
     */
    bool isHeatOn() const;

private:
    bool coolOn = false;  // État actuel de la sortie FROID
    bool heatOn = false;  // État actuel de la sortie CHAUD
};

#endif // RELAY_CONTROLLER_H