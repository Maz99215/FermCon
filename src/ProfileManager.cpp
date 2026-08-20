#include "ProfileManager.h"
#include "Config.h"
#include <time.h>
#include <strings.h>

#ifndef NTP_VALID_EPOCH_MIN
#define NTP_VALID_EPOCH_MIN 1600000000UL
#endif

ProfileStep::Type ProfileManager::parseStepType(JsonVariantConst v) {
    if (v.isNull()) return ProfileStep::PALIER;

    if (v.is<const char*>()) {
        const char* s = v.as<const char*>();
        if (s == nullptr) return ProfileStep::PALIER;
        if (strcasecmp(s, "RAMPE") == 0) return ProfileStep::RAMPE;
        if (strcasecmp(s, "RAMP")  == 0) return ProfileStep::RAMPE;
        if (strcasecmp(s, "1")     == 0) return ProfileStep::RAMPE;
        return ProfileStep::PALIER;
    }

    if (v.is<float>() || v.is<int>()) {
        return (v.as<int>() >= 1) ? ProfileStep::RAMPE : ProfileStep::PALIER;
    }

    return ProfileStep::PALIER;
}

void ProfileManager::start() {
    if (time(nullptr) > static_cast<time_t>(NTP_VALID_EPOCH_MIN)) {
        startEpoch     = static_cast<uint32_t>(time(nullptr));
        refMillis      = 0;
        refMillisValid = false;
    } else {
        startEpoch     = 0;
        refMillis      = millis();
        refMillisValid = true;
    }
    active = true;
}

void ProfileManager::stop()  { active = false; startEpoch = 0; }
void ProfileManager::pause() { active = false; }

bool ProfileManager::isActive() const  { return active; }
void ProfileManager::setActive(bool a) { active = a; }

void ProfileManager::setStartEpoch(uint32_t epoch) {
    startEpoch     = epoch;
    refMillis      = 0;
    refMillisValid = false;
}

bool ProfileManager::getElapsedS(uint32_t& elapsedOut) const {
    if (startEpoch > 0) {
        time_t now = time(nullptr);
        if (now > static_cast<time_t>(NTP_VALID_EPOCH_MIN)) {
            if (now >= static_cast<time_t>(startEpoch)) {
                elapsedOut = static_cast<uint32_t>(now) - startEpoch;
            } else {
                elapsedOut = 0;
            }
            return true;
        }
        return false;
    }

    if (refMillisValid) {
        elapsedOut = (millis() - refMillis) / 1000U;
        return true;
    }

    return false;
}

bool ProfileManager::isTimeReferenceValid() const {
    uint32_t dummy;
    return getElapsedS(dummy);
}

uint32_t ProfileManager::getStartEpoch() const { return startEpoch; }

// ---------------------------------------------------------------------------
// getFermentDays — NOUVEAU v0.4.0
// 0 si le lot est inactif ou l'horloge invalide, sinon jours ecoules
// ---------------------------------------------------------------------------
uint16_t ProfileManager::getFermentDays() const {
    if (!active) return 0;

    if (startEpoch > 0) {
        time_t now = time(nullptr);
        if (now > static_cast<time_t>(NTP_VALID_EPOCH_MIN)
            && now >= static_cast<time_t>(startEpoch)) {
            return static_cast<uint16_t>((now - startEpoch) / 86400);
        }
        return 0;
    }

    if (refMillisValid) {
        return static_cast<uint16_t>((millis() - refMillis) / 86400000UL);
    }

    return 0;
}

// ---------------------------------------------------------------------------
// getStageName — NOUVEAU v0.4.0
// label de l'etape courante, a defaut libelle automatique,
// nom du lot si stepCount == 0, chaine vide si inactif
// ---------------------------------------------------------------------------
String ProfileManager::getStageName() const {
    if (!active) return "";

    if (stepCount == 0) return name;

    uint8_t idx = getCurrentStepIndex();
    if (strlen(steps[idx].label) > 0) {
        return String(steps[idx].label);
    }

    return getCurrentStepInfo();
}

// ---------------------------------------------------------------------------
// drivesSetpoint — NOUVEAU v0.4.0 (ADR-011)
// Un lot actif sans etape ne pilote pas la consigne
// ---------------------------------------------------------------------------
bool ProfileManager::drivesSetpoint() const {
    return active && stepCount > 0;
}

// ---------------------------------------------------------------------------
// getCurrentStepIndex : retourne l'index de l'etape courante (0..stepCount-1)
// ---------------------------------------------------------------------------
uint8_t ProfileManager::getCurrentStepIndex() const {
    if (!active || stepCount == 0) return 0;

    uint32_t elapsed;
    if (!getElapsedS(elapsed)) return 0;

    uint32_t totalDuration = 0;
    for (uint8_t i = 0; i < stepCount; i++) {
        totalDuration += steps[i].durationS;
        if (elapsed < totalDuration) {
            return i;
        }
    }
    return stepCount - 1;
}

// ---------------------------------------------------------------------------
// getRemainingS : temps restant estime en secondes, -1 si inconnu
// ---------------------------------------------------------------------------
int32_t ProfileManager::getRemainingS() const {
    if (!active || stepCount == 0) return -1;

    uint32_t elapsed;
    if (!getElapsedS(elapsed)) return -1;

    uint32_t totalDuration = 0;
    for (uint8_t i = 0; i < stepCount; i++) {
        totalDuration += steps[i].durationS;
    }
    if (elapsed >= totalDuration) return 0;
    return (int32_t)(totalDuration - elapsed);
}

// ---------------------------------------------------------------------------
// getCurrentSetpoint — avec garde defensive durationS == 0
// ---------------------------------------------------------------------------
float ProfileManager::getCurrentSetpoint() const {
    if (!active || stepCount == 0) return DEFAULT_SETPOINT_C;

    uint32_t elapsed;
    if (!getElapsedS(elapsed)) return DEFAULT_SETPOINT_C;

    uint32_t totalDuration = 0;
    for (uint8_t i = 0; i < stepCount; i++) {
        uint32_t dur = steps[i].durationS;
        // Garde defensive : une etape avec durationS == 0 est traitee
        // comme un palier a tempStart (pas de division par zero).
        if (dur == 0) {
            Serial.printf("[PROFILE] AVERTISSEMENT: etape %u a durationS==0, traitee comme palier a %.1f C\n",
                          i, steps[i].tempStart);
            dur = 1; // evite la division par zero dans le calcul de progression
        }
        totalDuration += dur;
        if (elapsed < totalDuration) {
            if (steps[i].type == ProfileStep::PALIER) {
                return steps[i].tempStart;
            } else {
                uint32_t stepStart = totalDuration - dur;
                float progress = static_cast<float>(elapsed - stepStart)
                               / static_cast<float>(dur);
                return steps[i].tempStart
                     + (steps[i].tempEnd - steps[i].tempStart) * progress;
            }
        }
    }
    return steps[stepCount - 1].tempEnd;
}

// ---------------------------------------------------------------------------
// getCurrentStepInfo — libelle automatique (format inchange depuis v0.3.0)
// ---------------------------------------------------------------------------
String ProfileManager::getCurrentStepInfo() const {
    if (!active || stepCount == 0) return "Termine";

    uint32_t elapsed;
    if (!getElapsedS(elapsed)) return "Attente heure";

    uint32_t totalDuration = 0;
    for (uint8_t i = 0; i < stepCount; i++) {
        uint32_t dur = steps[i].durationS;
        if (dur == 0) dur = 1;
        totalDuration += dur;
        if (elapsed < totalDuration) {
            if (steps[i].type == ProfileStep::PALIER) {
                return "Palier " + String(steps[i].tempStart) + "C";
            } else {
                uint32_t stepStart = totalDuration - dur;
                float progress = static_cast<float>(elapsed - stepStart)
                               / static_cast<float>(dur);
                uint8_t progressPercent = static_cast<uint8_t>(progress * 100);
                return "Rampe " + String(steps[i].tempStart)
                     + "->" + String(steps[i].tempEnd)
                     + "C (" + String(progressPercent) + "%)";
            }
        }
    }
    return "Termine";
}

void ProfileManager::clearSteps() { stepCount = 0; }

bool ProfileManager::addStep(const ProfileStep& step) {
    if (stepCount >= 16) return false;
    steps[stepCount++] = step;
    return true;
}

void ProfileManager::setName(const String& n) { name = n; }

// ---------------------------------------------------------------------------
// toJson — v0.4.0 : ajout de schema_version et label par etape
// ---------------------------------------------------------------------------
void ProfileManager::toJson(JsonObject& json) const {
    json["schema_version"] = schemaVersion;
    json["name"] = name;
    JsonArray stepsArray = json["steps"].to<JsonArray>();
    for (uint8_t i = 0; i < stepCount; i++) {
        JsonObject step = stepsArray.add<JsonObject>();
        step["label"]     = steps[i].label;
        step["type"]      = (steps[i].type == ProfileStep::RAMPE) ? "RAMPE" : "PALIER";
        step["tempStart"] = steps[i].tempStart;
        step["tempEnd"]   = steps[i].tempEnd;
        step["durationS"] = steps[i].durationS;
    }
    json["startEpoch"] = startEpoch;
    json["startTime"]  = startEpoch;
    json["active"]     = active;
}

// ---------------------------------------------------------------------------
// fromJson — v0.4.0 : lecture du label et du schema_version
// Compatible v1 (schema_version absent, label absent) : label vide => auto
// ---------------------------------------------------------------------------
void ProfileManager::fromJson(const JsonObjectConst& json) {
    schemaVersion = json["schema_version"] | 1;  // defaut 1 pour fichiers v0.3.0
    name = json["name"] | "";

    JsonArrayConst stepsArray = json["steps"];
    size_t n = stepsArray.size();
    if (n > 16) n = 16;
    stepCount = static_cast<uint8_t>(n);
    for (uint8_t i = 0; i < stepCount; i++) {
        JsonObjectConst step = stepsArray[i];
        // label absent en v1 => chaine vide => libelle automatique
        const char* lbl = step["label"] | "";
        strlcpy(steps[i].label, lbl, sizeof(steps[i].label));
        steps[i].type      = parseStepType(step["type"]);
        steps[i].tempStart = step["tempStart"];
        steps[i].tempEnd   = step["tempEnd"];
        steps[i].durationS = step["durationS"];
    }

    startEpoch     = json["startEpoch"] | 0;
    refMillis      = 0;
    refMillisValid = false;

    active = json["active"] | active;
}
