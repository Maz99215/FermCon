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

    uint32_t startEpoch = 0;      // persiste
    uint32_t refMillis = 0;       // NON persiste, session courante
    bool     refMillisValid = false;  // NON persiste

    bool active = false;

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

    void toJson(JsonObject& json) const;
    void fromJson(const JsonObjectConst& json);
};