// include/DataPublisher.h
#ifndef DATA_PUBLISHER_H
#define DATA_PUBLISHER_H

#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

class DataPublisher {
public:
    DataPublisher();

    // Configuration MQTT
    void beginMqtt(const char* broker, int port, const char* user, const char* password, const char* topicPrefix, const char* deviceName);
    void setMqttEnabled(bool enabled);
    void loop();
    void publishFermenterTemp(float temp);
    void publishISpindel(float temp, float gravity, float angle, float battery);
    bool isMqttConnected();

    // Configuration Grainfather
    void configureGrainfather(const char* endpoint, const char* deviceLabel);
    void setGrainfatherEnabled(bool enabled);
    bool sendToGrainfather(const char* name, const char* id, float temperature, const char* tempUnits, float gravity, float angle, float battery, int rssi);

private:
    WiFiClient wifiClient;
    PubSubClient mqttClient;
    HTTPClient httpClient;

    bool mqttEnabled;
    bool grainfatherEnabled;
    char mqttTopicPrefix[64];
    char mqttDeviceName[32];
    char mqttUser[32];
    char mqttPassword[64];
    char grainfatherEndpoint[128];
    char grainfatherDeviceLabel[40];

    unsigned long lastMqttReconnectAttempt;
    const unsigned long mqttReconnectInterval = 5000; // 5 secondes

    void mqttReconnect();
};

#endif // DATA_PUBLISHER_H
