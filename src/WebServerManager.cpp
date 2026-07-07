#include "WebServerManager.h"
#include <ArduinoJson.h>

// SECURITE : config.password_hash stocke le mot de passe en clair (HTTP Basic,
// reseau local). Jamais renvoye par l'API.

WebServerManager::WebServerManager(ConfigStore* cfg, ISpindelReceiver* isp, TemperatureController* temp,
                                   RelayController* relay, SystemStatus* status,
                                   ProfileManager* profile, FermentationInfo* ferment)
    : server(80), configStore(cfg), ispindelReceiver(isp), temperatureController(temp),
      relayController(relay), systemStatus(status), profileManager(profile), fermentationInfo(ferment) {}

void WebServerManager::begin() {
    server.serveStatic("/", LittleFS, "/web/").setDefaultFile("index.html");

    // iSpindel : pas d'auth (c'est l'iSpindel qui poste)
    server.on("/ispindel", HTTP_POST, [](AsyncWebServerRequest* r) {}, NULL,
        [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t) { handleISpindel(r, d, l, i, t); });

    server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* r) {
        if (!authenticate(r)) return; handleStatus(r); });

    server.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* r) {
        if (!authenticate(r)) return; handleConfigGet(r); });

    server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest* r) {}, NULL,
        [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t) {
            if (!authenticate(r)) return; handleConfigPost(r, d, l, i, t); });

    server.on("/api/setpoint", HTTP_POST, [](AsyncWebServerRequest* r) {}, NULL,
        [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t) {
            if (!authenticate(r)) return; handleSetpoint(r, d, l, i, t); });

    server.on("/api/manual", HTTP_POST, [](AsyncWebServerRequest* r) {}, NULL,
        [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t) {
            if (!authenticate(r)) return; handleManualControl(r, d, l, i, t); });

    server.on("/api/profile", HTTP_GET, [this](AsyncWebServerRequest* r) {
        if (!authenticate(r)) return; handleProfileGet(r); });

    server.on("/api/profile", HTTP_POST, [](AsyncWebServerRequest* r) {}, NULL,
        [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t) {
            if (!authenticate(r)) return; handleProfilePost(r, d, l, i, t); });

    server.on("/api/profile/activate", HTTP_POST, [](AsyncWebServerRequest* r) {}, NULL,
        [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t) {
            if (!authenticate(r)) return; handleProfileActivate(r, d, l, i, t); });

    server.on("/api/fermentation", HTTP_GET, [this](AsyncWebServerRequest* r) {
        if (!authenticate(r)) return; handleFermentationGet(r); });

    server.on("/api/fermentation", HTTP_POST, [](AsyncWebServerRequest* r) {}, NULL,
        [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t) {
            if (!authenticate(r)) return; handleFermentationPost(r, d, l, i, t); });

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
    SystemConfig& c = configStore->getConfig();
    const char* user = (strlen(c.username) > 0) ? c.username : "admin";
    const char* pass = (strlen(c.password_hash) > 0) ? c.password_hash : "admin";
    if (!request->authenticate(user, pass)) {
        request->requestAuthentication();
        return false;
    }
    return true;
}

void WebServerManager::handleISpindel(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    static String body;
    if (index == 0) body = "";
    body += String((char*)data, len);
    if (index + len == total) {
        bool ok = ispindelReceiver->parsePayload(body);
        body = "";
        request->send(ok ? 200 : 400, "text/plain", ok ? "OK" : "ERR");
    }
}

void WebServerManager::handleStatus(AsyncWebServerRequest* request) {
    JsonDocument doc;
    doc["temperature"]    = systemStatus->temperature;
    doc["setpoint"]       = systemStatus->setpoint;
    doc["relay_fridge"]   = systemStatus->relay_fridge;
    doc["relay_heater"]   = systemStatus->relay_heater;
    doc["uptime"]         = systemStatus->uptime;
    doc["wifi_rssi"]      = systemStatus->wifi_rssi;
    doc["heap_free_kb"]   = systemStatus->heap_free_kb;
    doc["temp_sensor_ok"] = systemStatus->temp_sensor_ok;
    doc["ip_address"]     = systemStatus->ip_address;
    doc["isp_temperature"] = systemStatus->isp_temperature;
    doc["isp_gravity"]    = systemStatus->isp_gravity;
    doc["isp_battery"]    = systemStatus->isp_battery;
    doc["isp_angle"]      = systemStatus->isp_angle;
    doc["isp_last_update"] = systemStatus->isp_last_update;
    String out; serializeJson(doc, out);
    request->send(200, "application/json", out);
}

void WebServerManager::handleConfigGet(AsyncWebServerRequest* request) {
    JsonDocument doc;
    const SystemConfig& c = configStore->getConfig();
    doc["wifi_ssid"]            = c.wifi_ssid;
    doc["setpoint"]            = c.setpoint;
    doc["hysteresis"]          = c.hysteresis;
    doc["min_compressor_delay"] = c.min_compressor_delay;
    doc["temp_offset"]          = c.temp_offset;
    doc["username"]            = c.username;
    doc["mqtt_enabled"]         = c.mqtt_enabled;
    doc["mqtt_broker"]          = c.mqtt_broker;
    doc["mqtt_port"]            = c.mqtt_port;
    doc["mqtt_user"]            = c.mqtt_user;
    doc["mqtt_topic_prefix"]    = c.mqtt_topic_prefix;
    doc["gf_enabled"]           = c.gf_enabled;
    doc["gf_endpoint"]          = c.gf_endpoint;
    doc["gf_device_label"]      = c.gf_device_label;
    // mots de passe jamais renvoyes
    String out; serializeJson(doc, out);
    request->send(200, "application/json", out);
}

void WebServerManager::handleConfigPost(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    static String body;
    if (index == 0) body = "";
    body += String((char*)data, len);
    if (index + len != total) return;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    body = "";
    if (error) { request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }

    SystemConfig& c = configStore->getConfig();
    if (!doc["wifi_ssid"].isNull())            strlcpy(c.wifi_ssid, doc["wifi_ssid"], sizeof(c.wifi_ssid));
    if (!doc["wifi_password"].isNull())        strlcpy(c.wifi_password, doc["wifi_password"], sizeof(c.wifi_password));
    if (!doc["setpoint"].isNull())             c.setpoint = doc["setpoint"];
    if (!doc["hysteresis"].isNull())           c.hysteresis = doc["hysteresis"];
    if (!doc["min_compressor_delay"].isNull()) c.min_compressor_delay = doc["min_compressor_delay"];
    if (!doc["temp_offset"].isNull())          c.temp_offset = doc["temp_offset"];
    if (!doc["username"].isNull())             strlcpy(c.username, doc["username"], sizeof(c.username));
    if (!doc["password"].isNull())             strlcpy(c.password_hash, doc["password"], sizeof(c.password_hash));
    if (!doc["mqtt_enabled"].isNull())         c.mqtt_enabled = doc["mqtt_enabled"];
    if (!doc["mqtt_broker"].isNull())          strlcpy(c.mqtt_broker, doc["mqtt_broker"], sizeof(c.mqtt_broker));
    if (!doc["mqtt_port"].isNull())            c.mqtt_port = doc["mqtt_port"];
    if (!doc["mqtt_user"].isNull())            strlcpy(c.mqtt_user, doc["mqtt_user"], sizeof(c.mqtt_user));
    if (!doc["mqtt_password"].isNull())        strlcpy(c.mqtt_password, doc["mqtt_password"], sizeof(c.mqtt_password));
    if (!doc["mqtt_topic_prefix"].isNull())    strlcpy(c.mqtt_topic_prefix, doc["mqtt_topic_prefix"], sizeof(c.mqtt_topic_prefix));
    if (!doc["gf_enabled"].isNull())           c.gf_enabled = doc["gf_enabled"];
    if (!doc["gf_endpoint"].isNull())          strlcpy(c.gf_endpoint, doc["gf_endpoint"], sizeof(c.gf_endpoint));
    if (!doc["gf_device_label"].isNull())      strlcpy(c.gf_device_label, doc["gf_device_label"], sizeof(c.gf_device_label));

    if (configStore->saveConfig(c))
        request->send(200, "application/json", "{\"status\":\"success\"}");
    else
        request->send(500, "application/json", "{\"error\":\"Failed to save config\"}");
}

void WebServerManager::handleSetpoint(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    static String body;
    if (index == 0) body = "";
    body += String((char*)data, len);
    if (index + len != total) return;
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    body = "";
    if (!error && !doc["setpoint"].isNull()) {
        float sp = doc["setpoint"];
        temperatureController->setSetpoint(sp);
        JsonDocument r; r["status"] = "success"; r["setpoint"] = sp;
        String out; serializeJson(r, out);
        request->send(200, "application/json", out);
    } else {
        request->send(400, "text/plain", "Invalid request");
    }
}

void WebServerManager::handleManualControl(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    static String body;
    if (index == 0) body = "";
    body += String((char*)data, len);
    if (index + len != total) return;
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    body = "";
    if (!error) {
        bool cool = doc["cool"] | false;
        bool heat = doc["heat"] | false;
        relayController->setCool(cool);   // exclusivite geree dans RelayController
        relayController->setHeat(heat);
        JsonDocument r; r["status"] = "success"; r["cool"] = cool; r["heat"] = heat;
        String out; serializeJson(r, out);
        request->send(200, "application/json", out);
    } else {
        request->send(400, "text/plain", "Invalid request");
    }
}

void WebServerManager::handleProfileGet(AsyncWebServerRequest* request) {
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    profileManager->toJson(obj);
    obj["currentStep"] = profileManager->getCurrentStepInfo();
    obj["setpoint"]    = profileManager->getCurrentSetpoint();
    obj["active"]      = profileManager->isActive();
    String out; serializeJson(doc, out);
    request->send(200, "application/json", out);
}

void WebServerManager::handleProfilePost(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    static String body;
    if (index == 0) body = "";
    body += String((char*)data, len);
    if (index + len != total) return;
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    body = "";
    if (error) { request->send(400, "text/plain", "Invalid JSON"); return; }
    profileManager->fromJson(doc.as<JsonObjectConst>());
    configStore->saveProfile(*profileManager);
    request->send(200, "text/plain", "Profile saved");
}

void WebServerManager::handleProfileActivate(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    static String body;
    if (index == 0) body = "";
    body += String((char*)data, len);
    if (index + len != total) return;
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    body = "";
    if (error || doc["active"].isNull()) { request->send(400, "text/plain", "Invalid request"); return; }
    bool active = doc["active"];
    profileManager->setActive(active);
    if (active) profileManager->start(); else profileManager->stop();
    configStore->saveProfile(*profileManager);
    request->send(200, "text/plain", "Profile activation updated");
}

void WebServerManager::handleFermentationGet(AsyncWebServerRequest* request) {
    JsonDocument doc;
    doc["stageName"]   = fermentationInfo->getStageName();
    doc["fermentDays"] = fermentationInfo->getFermentDays();
    String out; serializeJson(doc, out);
    request->send(200, "application/json", out);
}

void WebServerManager::handleFermentationPost(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    static String body;
    if (index == 0) body = "";
    body += String((char*)data, len);
    if (index + len != total) return;
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    body = "";
    if (error) { request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }
    if (!doc["stageName"].isNull()) fermentationInfo->setStageName(doc["stageName"].as<String>());
    if (!doc["action"].isNull()) {
        String action = doc["action"].as<String>();
        if (action == "start") fermentationInfo->startBatch();
        else if (action == "reset") fermentationInfo->resetBatch();
    }
    configStore->saveFermentation(*fermentationInfo);
    JsonDocument resp;
    resp["stageName"]   = fermentationInfo->getStageName();
    resp["fermentDays"] = fermentationInfo->getFermentDays();
    String out; serializeJson(resp, out);
    request->send(200, "application/json", out);
}
