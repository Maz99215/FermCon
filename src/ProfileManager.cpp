#include "ProfileManager.h"
#include "Config.h"
#include <time.h>

#ifndef NTP_VALID_EPOCH_MIN
#define NTP_VALID_EPOCH_MIN 1600000000UL
#endif

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

void ProfileManager::stop()  { active = false; }
void ProfileManager::pause() { active = false; }

bool ProfileManager::isActive() const  { return active; }
void ProfileManager::setActive(bool a) { active = a; }

bool ProfileManager::getElapsedS(uint32_t& elapsedOut) const {
    if (startEpoch > 0) {
        time_t now = time(nullptr);
        if (now > static_cast<time_t>(NTP_VALID_EPOCH_MIN)) {
            if (now >= static_cast<time_t>(startEpoch)) {
                elapsedOut = static_cast<uint32_t>(now) - startEpoch;
            } else {
                elapsedOut = 0;   // horloge revenue en arriere
            }
            return true;
        }
        return false;   // epoch connu, heure pas encore synchronisee
    }

    if (refMillisValid) {
        elapsedOut = (millis() - refMillis) / 1000U;
        return true;
    }

    return false;   // aucune reference de temps exploitable
}

bool ProfileManager::isTimeReferenceValid() const {
    uint32_t dummy;
    return getElapsedS(dummy);
}

uint32_t ProfileManager::getStartEpoch() const { return startEpoch; }

float ProfileManager::getCurrentSetpoint() const {
    if (!active || stepCount == 0) return DEFAULT_SETPOINT_C;

    uint32_t elapsed;
    if (!getElapsedS(elapsed)) return DEFAULT_SETPOINT_C;

    uint32_t totalDuration = 0;
    for (uint8_t i = 0; i < stepCount; i++) {
        totalDuration += steps[i].durationS;
        if (elapsed < totalDuration) {
            if (steps[i].type == ProfileStep::PALIER) {
                return steps[i].tempStart;
            } else {
                uint32_t stepStart = totalDuration - steps[i].durationS;
                float progress = static_cast<float>(elapsed - stepStart)
                               / static_cast<float>(steps[i].durationS);
                return steps[i].tempStart
                     + (steps[i].tempEnd - steps[i].tempStart) * progress;
            }
        }
    }
    return steps[stepCount - 1].tempEnd;
}

String ProfileManager::getCurrentStepInfo() const {
    if (!active || stepCount == 0) return "Termine";

    uint32_t elapsed;
    if (!getElapsedS(elapsed)) return "Attente heure";

    uint32_t totalDuration = 0;
    for (uint8_t i = 0; i < stepCount; i++) {
        totalDuration += steps[i].durationS;
        if (elapsed < totalDuration) {
            if (steps[i].type == ProfileStep::PALIER) {
                return "Palier " + String(steps[i].tempStart) + "C";
            } else {
                uint32_t stepStart = totalDuration - steps[i].durationS;
                float progress = static_cast<float>(elapsed - stepStart)
                               / static_cast<float>(steps[i].durationS);
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

void ProfileManager::toJson(JsonObject& json) const {
    json["name"] = name;
    JsonArray stepsArray = json["steps"].to<JsonArray>();
    for (uint8_t i = 0; i < stepCount; i++) {
        JsonObject step = stepsArray.add<JsonObject>();
        step["type"]      = static_cast<int>(steps[i].type);
        step["tempStart"] = steps[i].tempStart;
        step["tempEnd"]   = steps[i].tempEnd;
        step["durationS"] = steps[i].durationS;
    }
    json["startEpoch"] = startEpoch;
    json["startTime"]  = startEpoch;   // compatibilite ascendante interface web
    json["active"]     = active;
}

void ProfileManager::fromJson(const JsonObjectConst& json) {
    name = json["name"] | "";

    JsonArrayConst stepsArray = json["steps"];
    size_t n = stepsArray.size();
    if (n > 16) n = 16;
    stepCount = static_cast<uint8_t>(n);
    for (uint8_t i = 0; i < stepCount; i++) {
        JsonObjectConst step = stepsArray[i];
        steps[i].type      = static_cast<ProfileStep::Type>(
                                 static_cast<int>(step["type"]));
        steps[i].tempStart = step["tempStart"];
        steps[i].tempEnd   = step["tempEnd"];
        steps[i].durationS = step["durationS"];
    }

    // L'ancienne cle "startTime" (valeur millis) est volontairement ignoree :
    // c'etait la cause du debordement.
    startEpoch     = json["startEpoch"] | 0;
    refMillis      = 0;
    refMillisValid = false;
    active         = json["active"];
}