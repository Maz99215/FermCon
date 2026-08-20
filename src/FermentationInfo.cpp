#include "FermentationInfo.h"
#include "Config.h"

FermentationInfo::FermentationInfo()
    : stageName("Fermentation"), startEpoch(0), refMillis(0), started(false) {}

void FermentationInfo::begin() {
}

void FermentationInfo::setStageName(const String& name) {
    stageName = name;
}

String FermentationInfo::getStageName() {
    return stageName;
}

void FermentationInfo::startBatch() {
    if (time(nullptr) > static_cast<time_t>(NTP_VALID_EPOCH_MIN)) {
        startEpoch = static_cast<uint32_t>(time(nullptr));
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
        time_t now = time(nullptr);
        if (now > static_cast<time_t>(NTP_VALID_EPOCH_MIN)) {
            return static_cast<uint16_t>((now - startEpoch) / 86400);
        }
        return 0;
    } else {
        return static_cast<uint16_t>((millis() - refMillis) / 86400000UL);
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
