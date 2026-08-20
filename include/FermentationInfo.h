#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <time.h>

class FermentationInfo {
public:
    FermentationInfo();

    void begin();
    void setStageName(const String& name);
    String getStageName();
    void startBatch();
    void resetBatch();
    uint16_t getFermentDays();
    void toJson(JsonObject obj) const;
    void fromJson(const JsonObjectConst& obj);
    bool isStarted() const { return started; }
    uint32_t getStartEpoch() const { return startEpoch; }

private:
    String stageName;
    uint32_t startEpoch;
    uint32_t refMillis;
    bool started;
};
