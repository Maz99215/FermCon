#ifndef CONFIG_VALIDATOR_H
#define CONFIG_VALIDATOR_H

#include "Config.h"
#include "ConfigStore.h"
#include "ProfileManager.h"

/**
 * Resultat de validation d'un champ de configuration.
 * ok = true si la valeur est dans les bornes.
 * field, code, message, min, max renseignes uniquement en cas d'erreur.
 */
struct ValidationResult {
    bool ok;
    const char* code;
    const char* field;
    const char* message;
    float min;
    float max;

    ValidationResult() : ok(true), code(nullptr), field(nullptr), message(nullptr), min(0), max(0) {}
};

/**
 * Valide une SystemConfig complete.
 * - Verifie les bornes individuelles de chaque parametre runtime.
 * - Verifie les contraintes croisees C1 a C5.
 * - Remplit ValidationResult en cas d'erreur.
 * Retourne true si la configuration est valide.
 */
bool validateConfig(const SystemConfig& config, ValidationResult& result);

/**
 * Ecrete silencieusement une SystemConfig aux bornes dures.
 * Chaque champ hors bornes est ramene a la borne la plus proche,
 * avec une trace serie nommant le champ, l'ancienne et la nouvelle valeur.
 * Les contraintes croisees C1 a C5 sont verifiees APRES l'ecretage individuel.
 */
void clampConfig(SystemConfig& config);

/**
 * Valide une etape de profil individuelle.
 * - Rejette durationS == 0
 * - Verifie les bornes de durationS (60..2592000)
 * - Verifie les bornes de temperature (0..35)
 * - Rejette un type inconnu
 * - field est de la forme "steps[N].<champ>"
 */
bool validateProfileStep(const ProfileStep& step, uint8_t index, ValidationResult& result);

/**
 * NOUVEAU v0.4.0 — Valide le label d'une etape.
 * - Longueur <= 23 caracteres (STEP_LABEL_MAX_LEN - 1)
 * - Rejette tout caractere de controle (< 0x20)
 * - field est de la forme "steps[N].label"
 */
bool validateStepLabel(const char* label, uint8_t index, ValidationResult& out);

#endif // CONFIG_VALIDATOR_H
