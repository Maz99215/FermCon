// src/ISpindelReceiver.cpp
#include "ISpindelReceiver.h"

bool ISpindelReceiver::parsePayload(const String& json) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);

    if (error) {
        return false;
    }

    // Validation des champs obligatoires
    if (doc["name"].isNull() || doc["ID"].isNull() ||
        doc["temperature"].isNull() || doc["gravity"].isNull() ||
        doc["battery"].isNull()) {
        return false;
    }

    _name = doc["name"].as<String>();
    _id = doc["ID"].as<String>();
    _temperature = doc["temperature"];
    _gravity = doc["gravity"];
    _angle = doc["angle"] | NAN;
    _battery = doc["battery"];
    _rssi = doc["RSSI"] | 0;
    _lastUpdate = millis();
    _newData = true;

    return true;
}

String ISpindelReceiver::getName() const { return _name; }
String ISpindelReceiver::getID() const { return _id; }
float ISpindelReceiver::getTemperature() const { return _temperature; }
float ISpindelReceiver::getGravity() const { return _gravity; }
float ISpindelReceiver::getAngle() const { return _angle; }
float ISpindelReceiver::getBattery() const { return _battery; }
int ISpindelReceiver::getRSSI() const { return _rssi; }
unsigned long ISpindelReceiver::getLastUpdate() const { return _lastUpdate; }

bool ISpindelReceiver::hasNewData() const { return _newData; }
void ISpindelReceiver::clearNewData() { _newData = false; }
