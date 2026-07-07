#include "ProfileManager.h"
#include "Config.h"

void ProfileManager::start() {
    startTime = millis();
    active = true;
}

void ProfileManager::stop() {
    active = false;
}

void ProfileManager::pause() {
    active = false;
}

bool ProfileManager::isActive() const {
    return active;
}

void ProfileManager::setActive(bool active) {
    this->active = active;
}

float ProfileManager::getCurrentSetpoint() const {
    // Repli sur: profil inactif ou vide -> consigne par defaut (jamais 0C)
    if (!active || stepCount == 0) return DEFAULT_SETPOINT_C;

    unsigned long elapsed = (millis() - startTime) / 1000;
    uint32_t totalDuration = 0;

    for (uint8_t i = 0; i < stepCount; i++) {
        totalDuration += steps[i].durationS;
        if (elapsed < totalDuration) {
            if (steps[i].type == ProfileStep::PALIER) {
                return steps[i].tempStart;
            } else {
                float progress = (float)(elapsed - (totalDuration - steps[i].durationS)) / steps[i].durationS;
                return steps[i].tempStart + (steps[i].tempEnd - steps[i].tempStart) * progress;
            }
        }
    }
    return steps[stepCount - 1].tempEnd;
}

String ProfileManager::getCurrentStepInfo() const {
    if (!active || stepCount == 0) return "Termine";

    unsigned long elapsed = (millis() - startTime) / 1000;
    uint32_t totalDuration = 0;

    for (uint8_t i = 0; i < stepCount; i++) {
        totalDuration += steps[i].durationS;
        if (elapsed < totalDuration) {
            if (steps[i].type == ProfileStep::PALIER) {
                return "Palier " + String(steps[i].tempStart) + "C";
            } else {
                float progress = (float)(elapsed - (totalDuration - steps[i].durationS)) / steps[i].durationS;
                uint8_t progressPercent = (uint8_t)(progress * 100);
                return "Rampe " + String(steps[i].tempStart) + "->" + String(steps[i].tempEnd) + "C (" + String(progressPercent) + "%)";
            }
        }
    }
    return "Termine";
}

void ProfileManager::clearSteps() {
    stepCount = 0;
}

bool ProfileManager::addStep(const ProfileStep& step) {
    if (stepCount >= 16) return false;
    steps[stepCount++] = step;
    return true;
}

void ProfileManager::setName(const String& name) {
    this->name = name;
}

void ProfileManager::toJson(JsonObject& json) const {
    json["name"] = name;
    JsonArray stepsArray = json["steps"].to<JsonArray>();
    for (uint8_t i = 0; i < stepCount; i++) {
        JsonObject step = stepsArray.add<JsonObject>();
        step["type"]      = (int)steps[i].type;
        step["tempStart"] = steps[i].tempStart;
        step["tempEnd"]   = steps[i].tempEnd;
        step["durationS"] = steps[i].durationS;
    }
    json["startTime"] = startTime;
    json["active"]    = active;
}

void ProfileManager::fromJson(const JsonObjectConst& json) {
    name = json["name"] | "";
    JsonArrayConst stepsArray = json["steps"];
    size_t n = stepsArray.size();
    if (n > 16) n = 16;                 // borne anti-overflow steps[16]
    stepCount = (uint8_t)n;
    for (uint8_t i = 0; i < stepCount; i++) {
        JsonObjectConst step = stepsArray[i];
        steps[i].type      = static_cast<ProfileStep::Type>((int)step["type"]);
        steps[i].tempStart = step["tempStart"];
        steps[i].tempEnd   = step["tempEnd"];
        steps[i].durationS = step["durationS"];
    }
    startTime = json["startTime"];
    active    = json["active"];
}
