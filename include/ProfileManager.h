#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "Config.h"

/**
 * ProfileStep — etape du lot.
 * ADR-012 : le vocabulaire public est "lot" et "etape", mais les noms de classe
 * et de fichier (/profile.json) sont conserves pour la compatibilite.
 */
struct ProfileStep {
    enum Type { PALIER, RAMPE } type;
    float tempStart;
    float tempEnd;
    uint32_t durationS;
    char label[STEP_LABEL_MAX_LEN];   // NOUVEAU v0.4.0 — nom libre, 0..23 car.
                                       // Vide => libelle automatique cote firmware.
};

/**
 * ProfileManager — entite Lot (ADR-010, ADR-012).
 * Porte le nom, les etapes, la reference temporelle unique et l'etat actif.
 * Les jours de fermentation, le libelle d'etape et la decision de consigne
 * sont des valeurs derivees, jamais persistees ni saisies.
 */
class ProfileManager {
private:
    String name;
    ProfileStep steps[16];
    uint8_t stepCount = 0;

    uint32_t startEpoch = 0;
    uint32_t refMillis = 0;
    bool     refMillisValid = false;

    bool active = false;

    uint8_t schemaVersion = PROFILE_SCHEMA_VERSION;  // NOUVEAU v0.4.0

    bool getElapsedS(uint32_t& elapsedOut) const;

public:
    void start();
    void stop();
    void pause();
    bool isActive() const;
    void setActive(bool active);
    float getCurrentSetpoint() const;
    String getCurrentStepInfo() const;
    void clearSteps();
    bool addStep(const ProfileStep& step);
    void setName(const String& name);

    bool isTimeReferenceValid() const;
    uint32_t getStartEpoch() const;
    void setStartEpoch(uint32_t epoch);             // NOUVEAU v0.4.0 — pour la migration

    uint8_t getSchemaVersion() const { return schemaVersion; }
    void setSchemaVersion(uint8_t v) { schemaVersion = v; }

    // Getters de progression (v0.3.0)
    uint8_t getStepCount() const { return stepCount; }
    uint8_t getCurrentStepIndex() const;
    int32_t getRemainingS() const;
    String getName() const { return name; }

    // NOUVEAU v0.4.0 — valeurs derivees du Lot
    uint16_t getFermentDays() const;                // 0 si inactif ou horloge invalide
    String   getStageName() const;                  // label courant, libelle auto, ou nom du lot
    bool     drivesSetpoint() const;                // active && stepCount > 0 (ADR-011)

    static ProfileStep::Type parseStepType(JsonVariantConst v);

    void toJson(JsonObject& json) const;
    void fromJson(const JsonObjectConst& json);
};
