// src/WebServerManager.cpp
#include "WebServerManager.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

// NOTE SECURITE: sur ce projet (reseau local), le champ config.password_hash
// stocke le mot de passe utilise pour l'authentification HTTP Basic (jamais
// renvoye par l'API). L'auth HTTP Basic transmet le mot de passe en clair,
// il ne peut donc pas etre compare a un hash cote serveur simplement.

WebServerManager::WebServerManager(ConfigStore* cfg, ISpindelReceiver* isp, TemperatureController* temp, RelayController* relay, SystemStatus* status)
    : server(80), configStore(cfg), ispindelReceiver(isp), temperatureController(temp), relayController(relay), systemStatus(status) {}

void WebServerManager::begin() {
    // Fichiers statiques de l'interface web
    server.serveStatic("/", LittleFS, "/web/").setDefaultFile("index.html");

    // Reception des donnees iSpindel (pas d'auth : c'est l'iSpindel qui poste)
    server.on("/ispindel", HTTP_POST,
        [this](AsyncWebServerRequest* request) {},
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            handleISpindel(request, data, len, index, total);
        });

    // Statut systeme
    server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!authenticate(request)) return;
        handleStatus(request);
    });

    // Configuration (lecture)
    server.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!authenticate(request)) return;
        handleConfigGet(request);
    });

    // Configuration (ecriture)
    server.on("/api/config", HTTP_POST,
        [this](AsyncWebServerRequest* request) {},
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (!authenticate(request)) return;
            handleConfigPost(request, data, len, index, total);
        });

    // Reglage consigne
    server.on("/api/setpoint", HTTP_POST,
        [this](AsyncWebServerRequest* request) {},
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (!authenticate(request)) return;
            handleSetpoint(request, data, len, index, total);
        });

    // Controle manuel des relais
    server.on("/api/manual", HTTP_POST,
        [this](AsyncWebServerRequest* request) {},
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (!authenticate(request)) return;
            handleManualControl(request, data, len, index, total);
        });

    // OTA (protege par les memes identifiants)
    SystemConfig& config = configStore->getConfig();
    ElegantOTA.begin(&server);
    if (strlen(config.password_hash) > 0) {
        ElegantOTA.setAuth(config.username, config.password_hash);
    }

    server.begin();
}

void WebServerManager::loop() {
    ElegantOTA.loop();
}

bool WebServerManager::authenticate(AsyncWebServerRequest* request) {
    SystemConfig& config = configStore->getConfig();
    const char* user = (strlen(config.username) > 0) ? config.username : "admin";
    const char* pass = (strlen(config.password_hash) > 0) ? config.password_hash : "admin";
    if (!request->authenticate(user, pass)) {
        request->requestAuthentication();
        return false;
    }
    return true;
}

void WebServerManager::handleISpindel(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    // Body suppose tenir en un seul chunk (payload iSpindel court)
    if (index == 0) {
        String body = String((char*)data, len);
        if (ispindelReceiver->parsePayload(body)) {
            request->send(200);
        } else {
            request->send(400);
        }
    }
}

void WebServerManager::handleStatus(AsyncWebServerRequest* request) {
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    JsonDocument doc;

    doc["temperature"] = systemStatus->temperature;
    doc["setpoint"] = systemStatus->setpoint;
    doc["relay_fridge"] = systemStatus->relay_fridge;
    doc["relay_heater"] = systemStatus->relay_heater;
    doc["uptime"] = systemStatus->uptime;
    doc["wifi_rssi"] = systemStatus->wifi_rssi;
    doc["heap_free_kb"] = systemStatus->heap_free_kb;
    doc["temp_sensor_ok"] = systemStatus->temp_sensor_ok;
    doc["ip_address"] = systemStatus->ip_address;
    doc["isp_temperature"] = systemStatus->isp_temperature;
    doc["isp_gravity"] = systemStatus->isp_gravity;
    doc["isp_battery"] = systemStatus->isp_battery;
    doc["isp_angle"] = systemStatus->isp_angle;
    doc["isp_last_update"] = systemStatus->isp_last_update;

    serializeJson(doc, *response);
    request->send(response);
}

void WebServerManager::handleConfigGet(AsyncWebServerRequest* request) {
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    JsonDocument doc;
    const SystemConfig& config = configStore->getConfig();

    doc["wifi_ssid"] = config.wifi_ssid;
    doc["setpoint"] = config.setpoint;
    doc["hysteresis"] = config.hysteresis;
    doc["min_compressor_delay"] = config.min_compressor_delay;
    doc["temp_offset"] = config.temp_offset;
    doc["username"] = config.username;
    doc["mqtt_enabled"] = config.mqtt_enabled;
    doc["mqtt_broker"] = config.mqtt_broker;
    doc["mqtt_port"] = config.mqtt_port;
    doc["mqtt_user"] = config.mqtt_user;
    doc["mqtt_topic_prefix"] = config.mqtt_topic_prefix;
    doc["gf_enabled"] = config.gf_enabled;
    doc["gf_endpoint"] = config.gf_endpoint;
    doc["gf_device_label"] = config.gf_device_label;
    // Les mots de passe ne sont JAMAIS renvoyes.

    serializeJson(doc, *response);
    request->send(response);
}

void WebServerManager::handleConfigPost(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    if (index == 0) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, data, len);

        if (error) {
            request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
            return;
        }

        SystemConfig& config = configStore->getConfig();

        if (!doc["wifi_ssid"].isNull())            strlcpy(config.wifi_ssid, doc["wifi_ssid"].as<const char*>(), sizeof(config.wifi_ssid));
        if (!doc["wifi_password"].isNull())        strlcpy(config.wifi_password, doc["wifi_password"].as<const char*>(), sizeof(config.wifi_password));
        if (!doc["setpoint"].isNull())             config.setpoint = doc["setpoint"];
        if (!doc["hysteresis"].isNull())           config.hysteresis = doc["hysteresis"];
        if (!doc["min_compressor_delay"].isNull()) config.min_compressor_delay = doc["min_compressor_delay"];
        if (!doc["temp_offset"].isNull())          config.temp_offset = doc["temp_offset"];
        if (!doc["username"].isNull())             strlcpy(config.username, doc["username"].as<const char*>(), sizeof(config.username));
        if (!doc["password"].isNull())             strlcpy(config.password_hash, doc["password"].as<const char*>(), sizeof(config.password_hash));
        if (!doc["mqtt_enabled"].isNull())         config.mqtt_enabled = doc["mqtt_enabled"];
        if (!doc["mqtt_broker"].isNull())          strlcpy(config.mqtt_broker, doc["mqtt_broker"].as<const char*>(), sizeof(config.mqtt_broker));
        if (!doc["mqtt_port"].isNull())            config.mqtt_port = doc["mqtt_port"];
        if (!doc["mqtt_user"].isNull())            strlcpy(config.mqtt_user, doc["mqtt_user"].as<const char*>(), sizeof(config.mqtt_user));
        if (!doc["mqtt_password"].isNull())        strlcpy(config.mqtt_password, doc["mqtt_password"].as<const char*>(), sizeof(config.mqtt_password));
        if (!doc["mqtt_topic_prefix"].isNull())    strlcpy(config.mqtt_topic_prefix, doc["mqtt_topic_prefix"].as<const char*>(), sizeof(config.mqtt_topic_prefix));
        if (!doc["gf_enabled"].isNull())           config.gf_enabled = doc["gf_enabled"];
        if (!doc["gf_endpoint"].isNull())          strlcpy(config.gf_endpoint, doc["gf_endpoint"].as<const char*>(), sizeof(config.gf_endpoint));
        if (!doc["gf_device_label"].isNull())      strlcpy(config.gf_device_label, doc["gf_device_label"].as<const char*>(), sizeof(config.gf_device_label));

        if (configStore->saveConfig(config)) {
            request->send(200, "application/json", "{\"status\":\"success\"}");
        } else {
            request->send(500, "application/json", "{\"error\":\"Failed to save config\"}");
        }
    }
}

void WebServerManager::handleSetpoint(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    if (index == 0) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, data, len);

        if (error || doc["value"].isNull()) {
            request->send(400, "application/json", "{\"error\":\"Invalid JSON or missing value\"}");
            return;
        }

        float value = doc["value"];
        temperatureController->setSetpoint(value);

        SystemConfig& config = configStore->getConfig();
        config.setpoint = value;
        configStore->saveConfig(config);

        request->send(200, "application/json", "{\"status\":\"success\"}");
    }
}

void WebServerManager::handleManualControl(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    if (index == 0) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, data, len);

        if (error || doc["fridge"].isNull() || doc["heater"].isNull()) {
            request->send(400, "application/json", "{\"error\":\"Invalid JSON or missing fridge/heater\"}");
            return;
        }

        bool fridge = doc["fridge"];
        bool heater = doc["heater"];

        // Securite : jamais les deux en meme temps
        if (fridge && heater) {
            request->send(400, "application/json", "{\"error\":\"fridge and heater cannot both be ON\"}");
            return;
        }

        relayController->setFridge(fridge);
        relayController->setHeater(heater);

        request->send(200, "application/json", "{\"status\":\"success\"}");
    }
}
