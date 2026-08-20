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

    // NOUVEAU v0.3.0 — Grainfather non bloquant (machine a etats)
    // Arme une requete d'envoi Grainfather. Les donnees sont copiees.
    bool requestGrainfatherSend(const char* name, const char* id,
                                float temperature, const char* tempUnits,
                                float gravity, float angle, float battery, int rssi);

    // Traite la requete Grainfather de facon asynchrone (a appeler depuis loop()).
    // Aucun delay() : la machine a etats gere le timeout et les retries.
    void loopGrainfather();

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
    const unsigned long mqttReconnectInterval = 5000;

    void mqttReconnect();

    // -----------------------------------------------------------------------
    // Machine a etats Grainfather
    // -----------------------------------------------------------------------
    enum GFState { GF_IDLE, GF_SENDING, GF_RETRY_WAIT };

    GFState gfState;
    unsigned long gfStateEntered;   // millis() a l'entree dans l'etat courant
    int gfRetryCount;
    static const int GF_MAX_RETRIES = 1;
    static const unsigned long GF_TIMEOUT_MS = 5000;      // timeout HTTP 5 s (endpoint externe sur Internet)
    static const unsigned long GF_RETRY_DELAY_MS = 5000;  // delai entre retries 5 s (respect rate-limit distant)

    // Donnees de la requete en cours
    char gfName[64];
    char gfId[32];
    float gfTemperature;
    char gfTempUnits[4];
    float gfGravity;
    float gfAngle;
    float gfBattery;
    int gfRssi;
};

#endif // DATA_PUBLISHER_H
