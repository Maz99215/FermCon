// src/DataPublisher.cpp
#include "DataPublisher.h"

DataPublisher::DataPublisher()
    : mqttClient(wifiClient),
      mqttEnabled(false), grainfatherEnabled(false),
      lastMqttReconnectAttempt(0),
      gfState(GF_IDLE), gfStateEntered(0), gfRetryCount(0) {
    mqttTopicPrefix[0] = '\0';
    mqttDeviceName[0] = '\0';
    mqttUser[0] = '\0';
    mqttPassword[0] = '\0';
    grainfatherEndpoint[0] = '\0';
    grainfatherDeviceLabel[0] = '\0';
    gfName[0] = '\0';
    gfId[0] = '\0';
    gfTempUnits[0] = '\0';
    gfTemperature = 0.0f;
    gfGravity = 0.0f;
    gfAngle = 0.0f;
    gfBattery = 0.0f;
    gfRssi = 0;
}

void DataPublisher::beginMqtt(const char* broker, int port, const char* user, const char* password, const char* topicPrefix, const char* deviceName) {
    mqttClient.setServer(broker, port);
    mqttClient.setCallback([](char* topic, byte* payload, unsigned int length) {
        // Callback MQTT (non utilise : publication uniquement)
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
    // Ajout du suffixe _SG si absent
    String label = String(deviceLabel);
    if (!label.endsWith("_SG")) {
        label += "_SG";
    }
    strlcpy(grainfatherDeviceLabel, label.c_str(), sizeof(grainfatherDeviceLabel));
}

void DataPublisher::setGrainfatherEnabled(bool enabled) {
    grainfatherEnabled = enabled;
    if (!enabled) {
        gfState = GF_IDLE; // annule toute requete en cours
    }
}

// ---------------------------------------------------------------------------
// requestGrainfatherSend : arme une requete non bloquante
// ---------------------------------------------------------------------------
bool DataPublisher::requestGrainfatherSend(const char* name, const char* id,
                                            float temperature, const char* tempUnits,
                                            float gravity, float angle, float battery, int rssi) {
    if (!grainfatherEnabled) return false;
    if (WiFi.status() != WL_CONNECTED) return false;

    // Si une requete est deja en cours, on l'ecrase (derniere mesure gagnante)
    strlcpy(gfName, (strlen(grainfatherDeviceLabel) > 0) ? grainfatherDeviceLabel : name, sizeof(gfName));
    strlcpy(gfId, id, sizeof(gfId));
    gfTemperature = temperature;
    strlcpy(gfTempUnits, tempUnits, sizeof(gfTempUnits));
    gfGravity = gravity;
    gfAngle = angle;
    gfBattery = battery;
    gfRssi = rssi;

    gfState = GF_SENDING;
    gfStateEntered = millis();
    gfRetryCount = 0;

    return true;
}

// ---------------------------------------------------------------------------
// loopGrainfather : machine a etats non bloquante
// Aucun delay() — appelee depuis loop() a chaque iteration.
// ---------------------------------------------------------------------------
void DataPublisher::loopGrainfather() {
    unsigned long now = millis();

    switch (gfState) {
    case GF_IDLE:
        // Rien a faire
        break;

    case GF_SENDING: {
        // Timeout de la tentative en cours ?
        if (now - gfStateEntered > GF_TIMEOUT_MS) {
            // Timeout : passer en retry
            gfRetryCount++;
            Serial.printf("[GF] timeout tentative %d/%d\n", gfRetryCount, GF_MAX_RETRIES + 1);
            if (gfRetryCount <= GF_MAX_RETRIES) {
                gfState = GF_RETRY_WAIT;
                gfStateEntered = now;
            } else {
                Serial.println("[GF] echec definitif apres retries");
                gfState = GF_IDLE;
            }
            break;
        }

        // Construire le payload JSON
        JsonDocument doc;
        doc["name"] = gfName;
        doc["ID"] = gfId;
        doc["temperature"] = gfTemperature;
        doc["temp_units"] = gfTempUnits;
        doc["gravity"] = gfGravity;
        doc["angle"] = gfAngle;
        doc["battery"] = gfBattery;
        doc["RSSI"] = gfRssi;

        char payload[512];
        serializeJson(doc, payload, sizeof(payload));

        // Tentative d'envoi HTTP (non bloquante car timeout court)
        httpClient.begin(grainfatherEndpoint);
        httpClient.setTimeout(GF_TIMEOUT_MS); // aligne sur le timeout global de la machine a etats
        httpClient.addHeader("Content-Type", "application/json");

        int httpCode = httpClient.POST((uint8_t*)payload, strlen(payload));
        httpClient.end();

        if (httpCode >= 200 && httpCode < 300) {
            Serial.println("[GF] envoi reussi");
            gfState = GF_IDLE;
        } else if (httpCode == 429) {
            // Rate-limit distant : la mesure precedente a probablement ete recue.
            // On ne retente pas immediatement pour ne pas aggraver le blocage cote serveur.
            Serial.println("[GF] HTTP 429 (rate-limit) - abandon sans retry, prochaine mesure iSpindel reessaiera");
            gfState = GF_IDLE;
        } else {
            Serial.printf("[GF] echec HTTP %d, tentative %d/%d\n",
                          httpCode, gfRetryCount + 1, GF_MAX_RETRIES + 1);
            gfRetryCount++;
            if (gfRetryCount <= GF_MAX_RETRIES) {
                gfState = GF_RETRY_WAIT;
                gfStateEntered = now;
            } else {
                Serial.println("[GF] echec definitif apres retries");
                gfState = GF_IDLE;
            }
        }
        break;
    }

    case GF_RETRY_WAIT:
        // Attente non bloquante avant nouvelle tentative
        if (now - gfStateEntered >= GF_RETRY_DELAY_MS) {
            gfState = GF_SENDING;
            gfStateEntered = now;
        }
        break;
    }
}

void DataPublisher::mqttReconnect() {
    const char* user = (strlen(mqttUser) > 0) ? mqttUser : nullptr;
    const char* pass = (strlen(mqttPassword) > 0) ? mqttPassword : nullptr;
    mqttClient.connect(mqttDeviceName, user, pass);
}
