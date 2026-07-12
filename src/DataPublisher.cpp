// src/DataPublisher.cpp
#include "DataPublisher.h"

DataPublisher::DataPublisher() : mqttClient(wifiClient) {
    mqttEnabled = false;
    grainfatherEnabled = false;
    lastMqttReconnectAttempt = 0;
    mqttTopicPrefix[0] = '\0';
    mqttDeviceName[0] = '\0';
    mqttUser[0] = '\0';
    mqttPassword[0] = '\0';
    grainfatherEndpoint[0] = '\0';
    grainfatherDeviceLabel[0] = '\0';
}

void DataPublisher::beginMqtt(const char* broker, int port, const char* user, const char* password, const char* topicPrefix, const char* deviceName) {
    mqttClient.setServer(broker, port);
    mqttClient.setCallback([](char* topic, byte* payload, unsigned int length) {
        // Callback MQTT (non utilisé : publication uniquement)
    });

    strlcpy(mqttTopicPrefix, topicPrefix, sizeof(mqttTopicPrefix));
    strlcpy(mqttDeviceName, deviceName, sizeof(mqttDeviceName));
    strlcpy(mqttUser, user ? user : "", sizeof(mqttUser));
    strlcpy(mqttPassword, password ? password : "", sizeof(mqttPassword));

    mqttEnabled = true;
}

void DataPublisher::setMqttEnabled(bool enabled) {
    mqttEnabled = enabled;
}

void DataPublisher::loop() {
    if (mqttEnabled) {
        if (!mqttClient.connected()) {
            unsigned long now = millis();
            if (now - lastMqttReconnectAttempt > mqttReconnectInterval) {
                lastMqttReconnectAttempt = now;
                mqttReconnect();
            }
        } else {
            mqttClient.loop();
        }
    }
}

void DataPublisher::publishFermenterTemp(float temp) {
    if (mqttEnabled && mqttClient.connected()) {
        char topic[128];
        snprintf(topic, sizeof(topic), "%s/temperature", mqttTopicPrefix);

        char payload[64];
        snprintf(payload, sizeof(payload), "%.2f", temp);

        mqttClient.publish(topic, payload);
    }
}

void DataPublisher::publishISpindel(float temp, float gravity, float angle, float battery) {
    if (mqttEnabled && mqttClient.connected()) {
        char topic[160];
        char payload[32];

        snprintf(topic, sizeof(topic), "%s/ispindel/temperature", mqttTopicPrefix);
        snprintf(payload, sizeof(payload), "%.2f", temp);
        mqttClient.publish(topic, payload);

        snprintf(topic, sizeof(topic), "%s/ispindel/gravity", mqttTopicPrefix);
        snprintf(payload, sizeof(payload), "%.4f", gravity);
        mqttClient.publish(topic, payload);

        snprintf(topic, sizeof(topic), "%s/ispindel/angle", mqttTopicPrefix);
        snprintf(payload, sizeof(payload), "%.2f", angle);
        mqttClient.publish(topic, payload);

        snprintf(topic, sizeof(topic), "%s/ispindel/battery", mqttTopicPrefix);
        snprintf(payload, sizeof(payload), "%.2f", battery);
        mqttClient.publish(topic, payload);
    }
}

bool DataPublisher::isMqttConnected() {
    return mqttClient.connected();
}

void DataPublisher::configureGrainfather(const char* endpoint, const char* deviceLabel) {
    strlcpy(grainfatherEndpoint, endpoint, sizeof(grainfatherEndpoint));
    // Ajout du suffixe _SG si absent (pour forcer les unités Specific Gravity)
    String label = String(deviceLabel);
    if (!label.endsWith("_SG")) {
        label += "_SG";
    }
    strlcpy(grainfatherDeviceLabel, label.c_str(), sizeof(grainfatherDeviceLabel));
}

void DataPublisher::setGrainfatherEnabled(bool enabled) {
    grainfatherEnabled = enabled;
}

bool DataPublisher::sendToGrainfather(const char* name, const char* id, float temperature, const char* tempUnits, float gravity, float angle, float battery, int rssi) {
    if (!grainfatherEnabled) return false;
    if (WiFi.status() != WL_CONNECTED) return false;

    JsonDocument doc;
    // Le champ "name" doit porter le label Grainfather (avec suffixe _SG)
    doc["name"] = (strlen(grainfatherDeviceLabel) > 0) ? grainfatherDeviceLabel : name;
    doc["ID"] = id;
    doc["temperature"] = temperature;
    doc["temp_units"] = tempUnits;
    doc["gravity"] = gravity;
    doc["angle"] = angle;
    doc["battery"] = battery;
    doc["RSSI"] = rssi;

    char payload[512];
    serializeJson(doc, payload, sizeof(payload));

    bool success = false;
    int retryCount = 0;
    const int maxRetries = 3;

    while (!success && retryCount < maxRetries) {
        httpClient.begin(grainfatherEndpoint);
        httpClient.setTimeout(5000); // 5 s
        httpClient.addHeader("Content-Type", "application/json");

        int httpCode = httpClient.POST((uint8_t*)payload, strlen(payload));
        success = (httpCode >= 200 && httpCode < 300);
        httpClient.end();

        if (!success) {
            retryCount++;
            if (retryCount < maxRetries) {
                delay(500); // courte attente entre essais
            }
        }
    }

    return success;
}

void DataPublisher::mqttReconnect() {
    const char* user = (strlen(mqttUser) > 0) ? mqttUser : nullptr;
    const char* pass = (strlen(mqttPassword) > 0) ? mqttPassword : nullptr;
    mqttClient.connect(mqttDeviceName, user, pass);
}
