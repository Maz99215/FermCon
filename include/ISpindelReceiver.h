// include/ISpindelReceiver.h
#ifndef ISPINDEL_RECEIVER_H
#define ISPINDEL_RECEIVER_H

#include <Arduino.h>
#include <ArduinoJson.h>

class ISpindelReceiver {
public:
    bool parsePayload(const String& json);
    String getName() const;
    String getID() const;
    float getTemperature() const;
    float getGravity() const;
    float getAngle() const;
    float getBattery() const;
    int getRSSI() const;
    unsigned long getLastUpdate() const;

    bool hasNewData() const;
    void clearNewData();

private:
    String _name;
    String _id;
    float _temperature = NAN;
    float _gravity = NAN;
    float _angle = NAN;
    float _battery = NAN;
    int _rssi = 0;
    unsigned long _lastUpdate = 0;
    bool _newData = false;
};

#endif // ISPINDEL_RECEIVER_H
