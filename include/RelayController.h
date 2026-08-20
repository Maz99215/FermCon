#ifndef RELAY_CONTROLLER_H
#define RELAY_CONTROLLER_H

#include <Arduino.h>
#include "Config.h"

#ifndef RELAY_KEEPALIVE_TIMEOUT_S
#define RELAY_KEEPALIVE_TIMEOUT_S 30
#endif

/**
 * Controleur des deux sorties de puissance du systeme de fermentation FermCon.
 *
 * Materiel : module relais 2 canaux opto-isole, LES DEUX CANAUX ACTIFS LOW
 * (entree IN au niveau bas = relais enclenche). Fait verifie sur piece.
 *   - GPIO2 (PIN_COOL) : voie FROID, compresseur du refrigerateur.
 *   - GPIO3 (PIN_HEAT) : voie CHAUD, plaque chauffante 25 W.
 *
 * Les niveaux actifs sont definis EXCLUSIVEMENT dans Config.h
 * (COOL_ACTIVE_LEVEL et HEAT_ACTIVE_LEVEL). Cette classe n'ecrit jamais
 * HIGH ou LOW en dur.
 *
 * Garanties apportees :
 *   - Etat inactif etabli au demarrage sans aucun enclenchement transitoire.
 *   - Exclusivite structurelle : les deux sorties ne peuvent jamais etre
 *     actives simultanement, quel que soit le chemin d'appel.
 *   - Sentinelle de vivacite : sans rafraichissement de la boucle de
 *     regulation, les sorties sont coupees.
 */
class RelayController {
public:
    /**
     * Initialise les broches et etablit l'etat inactif des deux sorties.
     *
     * Le niveau inactif est ecrit dans le registre de sortie AVANT
     * pinMode(OUTPUT), puis reecrit apres. Sur ESP32, pinMode(OUTPUT) force
     * la broche a 0 V : avec un module actif LOW, cela enclencherait
     * brievement le relais. Cette sequence supprime le transitoire.
     *
     * Arme egalement la sentinelle de vivacite.
     */
    void begin();

    /**
     * Active ou desactive la sortie FROID.
     * Si on == true, la sortie CHAUD est coupee d'abord (exclusivite).
     */
    void setCool(bool on);

    /**
     * Active ou desactive la sortie CHAUD.
     * Si on == true, la sortie FROID est coupee d'abord (exclusivite).
     */
    void setHeat(bool on);

    /**
     * Coupe les deux sorties (repli sur).
     */
    void allOff();

    /** @return true si la sortie FROID est active. */
    bool isCoolOn() const;

    /** @return true si la sortie CHAUD est active. */
    bool isHeatOn() const;

    /**
     * Signale que la boucle de regulation a effectue un passage complet.
     * A appeler apres chaque cycle de regulation abouti.
     */
    void keepAlive();

    /**
     * Verifie la sentinelle de vivacite. A appeler depuis la boucle principale.
     *
     * Si aucun keepAlive() n'a eu lieu depuis plus de
     * RELAY_KEEPALIVE_TIMEOUT_S secondes ET qu'au moins une sortie est active,
     * les deux sorties sont coupees et l'evenement est journalise.
     *
     * @return true si une coupure a ete declenchee par expiration du delai.
     */
    bool checkKeepAlive();

private:
    bool coolOn = false;              // Etat actuel de la sortie FROID
    bool heatOn = false;              // Etat actuel de la sortie CHAUD
    unsigned long lastKeepAliveMs = 0;

    /**
     * Applique l'etat demande aux deux sorties. SEULE methode qui ecrit
     * sur les broches.
     *
     * Si cool et heat valent tous deux true, les DEUX sorties sont coupees
     * et une erreur est journalisee : cet appel signale un defaut logique
     * de l'appelant.
     *
     * Les coupures sont ecrites INCONDITIONNELLEMENT et AVANT toute
     * activation, afin qu'aucun recouvrement ne soit possible et qu'une
     * divergence entre l'etat bufferise et l'etat materiel ne puisse pas
     * empecher une coupure.
     */
    void applyOutputs(bool cool, bool heat);
};

#endif // RELAY_CONTROLLER_H
