#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

struct ProfileStep {
    enum Type { PALIER, RAMPE } type;
    float tempStart;
    float tempEnd;
    uint32_t durationS;
};

class ProfileManager {
private:
    String name;
    ProfileStep steps[16];
    uint8_t stepCount = 0;
    unsigned long startTime = 0;
    bool active = false;

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
    void toJson(JsonObject& json) const;
    void fromJson(const JsonObjectConst& json);
};
