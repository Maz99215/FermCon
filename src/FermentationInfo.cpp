#include "FermentationInfo.h"

FermentationInfo::FermentationInfo()
    : stageName("Fermentation"), startEpoch(0), refMillis(0), started(false) {}

void FermentationInfo::begin() {
    // Initialisation si nécessaire
}

void FermentationInfo::setStageName(const String& name) {
    stageName = name;
}

String FermentationInfo::getStageName() {
    return stageName;
}

void FermentationInfo::startBatch() {
    if (time(nullptr) > 1600000000) { // Vérification de la validité du timestamp NTP
        startEpoch = time(nullptr);
        refMillis = 0;
    } else {
        startEpoch = 0;
        refMillis = millis();
    }
    started = true;
}

void FermentationInfo::resetBatch() {
    startEpoch = 0;
    refMillis = 0;
    started = false;
}

uint16_t FermentationInfo::getFermentDays() {
    if (!started) return 0;
    
    if (startEpoch > 0) {
        return (time(nullptr) - startEpoch) / 86400;
    } else {
        return (millis() - refMillis) / 86400000UL;
    }
}

void FermentationInfo::toJson(JsonObject obj) const {
    obj["stageName"] = stageName;
    obj["startEpoch"] = startEpoch;
    obj["refMillis"] = refMillis;
    obj["started"] = started;
}

void FermentationInfo::fromJson(const JsonObjectConst& obj) {
    stageName = obj["stageName"] | "Fermentation";
    startEpoch = obj["startEpoch"] | 0;
    refMillis = obj["refMillis"] | 0;
    started = obj["started"] | false;
}
