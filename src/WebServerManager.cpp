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

    server.on("/api/profile/activate", HTTP_POST, [](AsyncWebServerRequest* r) {}, NULL,
        [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t) {
            if (!authenticate(r)) return; handleProfileActivate(r, d, l, i, t); });

    server.on("/api/profile", HTTP_POST, [](AsyncWebServerRequest* r) {}, NULL,
        [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t) {
            if (!authenticate(r)) return; handleProfilePost(r, d, l, i, t); });

    server.on("/api/fermentation", HTTP_GET, [this](AsyncWebServerRequest* r) {
        if (!authenticate(r)) return; handleFermentationGet(r); });

    server.on("/api/fermentation", HTTP_POST, [](AsyncWebServerRequest* r) {}, NULL,
        [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t) {
            if (!authenticate(r)) return; handleFermentationPost(r, d, l, i, t); });

    // Redemarrage differe via API
    server.on("/api/restart", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (!authenticate(request)) return;
        handleRestart(request);
    });

    SystemConfig& config = configStore->getConfig();
    ElegantOTA.begin(&server);
    if (strlen(config.password_hash) > 0) {
        ElegantOTA.setAuth(config.username, config.password_hash);
    }
    server.begin();
}

void WebServerManager::loop() {
    ElegantOTA.loop();
    // Redemarrage differe, hors contexte async
    if (_restartRequested && (long)(millis() - _restartDeadline) >= 0) {
        Serial.println("[WEB] Redemarrage demande via API");
        Serial.flush();
        ESP.restart();
    }
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
    // Tampon par requete via _tempObject (corrige la concurrence du static String)
    // malloc/free : le destructeur de AsyncWebServerRequest libere _tempObject avec free()
    if (index == 0) {
        if (request->_tempObject != nullptr) {
            free(request->_tempObject);
            request->_tempObject = nullptr;
        }
        request->_tempObject = malloc(total + 1);
        if (!request->_tempObject) {
            request->send(500, "text/plain", "ERR");
            return;
        }
    }

    char* buf = (char*)request->_tempObject;
    if (!buf) {
        request->send(500, "text/plain", "ERR");
        return;
    }
    memcpy(buf + index, data, len);

    if (index + len == total) {
        buf[total] = '\0';
        bool ok = ispindelReceiver->parsePayload(String(buf));
        free(buf);
        request->_tempObject = nullptr;
        request->send(ok ? 200 : 400, "text/plain", ok ? "OK" : "ERR");
    }
}

void WebServerManager::handleStatus(AsyncWebServerRequest* request) {
    JsonDocument doc;
    doc["temperature"]     = systemStatus->temperature;
    doc["setpoint"]        = systemStatus->setpoint;
    doc["relay_fridge"]    = systemStatus->relay_fridge;
    doc["relay_heater"]    = systemStatus->relay_heater;
    doc["uptime"]          = systemStatus->uptime;
    doc["wifi_rssi"]       = systemStatus->wifi_rssi;
    doc["heap_free_kb"]    = systemStatus->heap_free_kb;
    doc["temp_sensor_ok"]  = systemStatus->temp_sensor_ok;
    doc["ip_address"]      = systemStatus->ip_address;
    doc["isp_temperature"] = systemStatus->isp_temperature;
    doc["isp_gravity"]     = systemStatus->isp_gravity;
    doc["isp_battery"]     = systemStatus->isp_battery;
    doc["isp_angle"]       = systemStatus->isp_angle;
    doc["isp_last_update"] = systemStatus->isp_last_update;
    doc["sta_connected"]   = systemStatus->sta_connected;
    doc["ip_sta"]          = systemStatus->ip_sta;
    doc["ip_ap"]           = systemStatus->ip_ap;
    doc["ap_clients"]      = systemStatus->ap_clients;
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
    doc["ap_enabled"]      = c.ap_enabled;
    doc["ap_ssid"]         = c.ap_ssid;
    doc["ap_password_set"] = (strlen(c.ap_password) > 0);
    String out; serializeJson(doc, out);
    request->send(200, "application/json", out);
}

void WebServerManager::handleConfigPost(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
// Tampon par requete via _tempObject (corrige la concurrence du static String)
    // malloc/free : le destructeur de AsyncWebServerRequest libere _tempObject avec free()
    if (index == 0) {
        if (request->_tempObject != nullptr) {
            free(request->_tempObject);
            request->_tempObject = nullptr;
        }
        request->_tempObject = malloc(total + 1);
        if (!request->_tempObject) {
            request->send(500, "application/json", "{\"error\":\"Allocation failed\"}");
            return;
        }
    }

    char* buf = (char*)request->_tempObject;
    if (!buf) {
        request->send(500, "application/json", "{\"error\":\"Internal error\"}");
        return;
    }
    memcpy(buf + index, data, len);

    if (index + len != total) return;
    buf[total] = '\0';

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (const char*)buf);

    // Nettoyer avant de repondre
    free(buf);
    request->_tempObject = nullptr;

    if (error) { request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }

    // Copie locale : une validation en echec ne doit pas corrompre la config en RAM
    SystemConfig tmp = configStore->getConfig();

    if (!doc["wifi_ssid"].isNull())            strlcpy(tmp.wifi_ssid, doc["wifi_ssid"], sizeof(tmp.wifi_ssid));
    if (!doc["wifi_password"].isNull())        strlcpy(tmp.wifi_password, doc["wifi_password"], sizeof(tmp.wifi_password));
    if (!doc["setpoint"].isNull())             tmp.setpoint = doc["setpoint"];
    if (!doc["hysteresis"].isNull())           tmp.hysteresis = doc["hysteresis"];
    if (!doc["min_compressor_delay"].isNull()) tmp.min_compressor_delay = doc["min_compressor_delay"];
    if (!doc["temp_offset"].isNull())          tmp.temp_offset = doc["temp_offset"];
    if (!doc["username"].isNull())             strlcpy(tmp.username, doc["username"], sizeof(tmp.username));
    if (!doc["password"].isNull())             strlcpy(tmp.password_hash, doc["password"], sizeof(tmp.password_hash));
    if (!doc["mqtt_enabled"].isNull())         tmp.mqtt_enabled = doc["mqtt_enabled"];
    if (!doc["mqtt_broker"].isNull())          strlcpy(tmp.mqtt_broker, doc["mqtt_broker"], sizeof(tmp.mqtt_broker));
    if (!doc["mqtt_port"].isNull())            tmp.mqtt_port = doc["mqtt_port"];
    if (!doc["mqtt_user"].isNull())            strlcpy(tmp.mqtt_user, doc["mqtt_user"], sizeof(tmp.mqtt_user));
    if (!doc["mqtt_password"].isNull())        strlcpy(tmp.mqtt_password, doc["mqtt_password"], sizeof(tmp.mqtt_password));
    if (!doc["mqtt_topic_prefix"].isNull())    strlcpy(tmp.mqtt_topic_prefix, doc["mqtt_topic_prefix"], sizeof(tmp.mqtt_topic_prefix));
    if (!doc["gf_enabled"].isNull())           tmp.gf_enabled = doc["gf_enabled"];
    if (!doc["gf_endpoint"].isNull())          strlcpy(tmp.gf_endpoint, doc["gf_endpoint"], sizeof(tmp.gf_endpoint));
    if (!doc["gf_device_label"].isNull())      strlcpy(tmp.gf_device_label, doc["gf_device_label"], sizeof(tmp.gf_device_label));

    // Champs AP + suivi du redemarrage necessaire
    bool reboot_required = false;
    if (!doc["ap_enabled"].isNull())  { tmp.ap_enabled = doc["ap_enabled"];                         reboot_required = true; }
    if (!doc["ap_ssid"].isNull())     { strlcpy(tmp.ap_ssid, doc["ap_ssid"], sizeof(tmp.ap_ssid));  reboot_required = true; }
    if (!doc["ap_password"].isNull()) {
        const char* pwd = doc["ap_password"] | "";
        if (strlen(pwd) > 0) strlcpy(tmp.ap_password, pwd, sizeof(tmp.ap_password));
        // chaine vide = ne pas changer le mot de passe
        reboot_required = true;
    }

    // Validation AP avant sauvegarde
    if (tmp.ap_enabled) {
        if (strlen(tmp.ap_ssid) == 0) {
            request->send(400, "application/json", "{\"error\":\"ap_ssid vide\"}");
            return;
        }
        if (strlen(tmp.ap_password) < AP_MIN_PASSWORD_LEN) {
            request->send(400, "application/json", "{\"error\":\"mot de passe AP trop court (8 caracteres minimum)\"}");
            return;
        }
    }

    if (configStore->saveConfig(tmp)) {
        String response = "{\"status\":\"success\",\"reboot_required\":";
        response += reboot_required ? "true}" : "false}";
        request->send(200, "application/json", response);
    } else {
        request->send(500, "application/json", "{\"error\":\"Failed to save config\"}");
    }
}

void WebServerManager::handleSetpoint(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    // Tampon par requete via _tempObject (corrige la concurrence du static String)
    // malloc/free : le destructeur de AsyncWebServerRequest libere _tempObject avec free()
    if (index == 0) {
        if (request->_tempObject != nullptr) {
            free(request->_tempObject);
            request->_tempObject = nullptr;
        }
        request->_tempObject = malloc(total + 1);
        if (!request->_tempObject) {
            request->send(500, "text/plain", "Allocation failed");
            return;
        }
    }

    char* buf = (char*)request->_tempObject;
    if (!buf) {
        request->send(500, "text/plain", "Internal error");
        return;
    }
    memcpy(buf + index, data, len);

    if (index + len != total) return;
    buf[total] = '\0';

    JsonDocument doc;
    // Cast const char* : force le mode copie, le document devient proprietaire de ses chaines
    DeserializationError error = deserializeJson(doc, (const char*)buf);
    free(buf);
    request->_tempObject = nullptr;

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
    // Tampon par requete via _tempObject (corrige la concurrence du static String)
    // malloc/free : le destructeur de AsyncWebServerRequest libere _tempObject avec free()
    if (index == 0) {
        if (request->_tempObject != nullptr) {
            free(request->_tempObject);
            request->_tempObject = nullptr;
        }
        request->_tempObject = malloc(total + 1);
        if (!request->_tempObject) {
            request->send(500, "text/plain", "Allocation failed");
            return;
        }
    }

    char* buf = (char*)request->_tempObject;
    if (!buf) {
        request->send(500, "text/plain", "Internal error");
        return;
    }
    memcpy(buf + index, data, len);

    if (index + len != total) return;
    buf[total] = '\0';

    JsonDocument doc;
    // Cast const char* : force le mode copie, le document devient proprietaire de ses chaines
    DeserializationError error = deserializeJson(doc, (const char*)buf);
    free(buf);
    request->_tempObject = nullptr;

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
    // Tampon par requete via _tempObject (corrige la concurrence du static String)
    // malloc/free : le destructeur de AsyncWebServerRequest libere _tempObject avec free()
    if (index == 0) {
        if (request->_tempObject != nullptr) {
            free(request->_tempObject);
            request->_tempObject = nullptr;
        }
        request->_tempObject = malloc(total + 1);
        if (!request->_tempObject) {
            request->send(500, "text/plain", "Allocation failed");
            return;
        }
    }

    char* buf = (char*)request->_tempObject;
    if (!buf) {
        request->send(500, "text/plain", "Internal error");
        return;
    }
    memcpy(buf + index, data, len);

    if (index + len != total) return;
    buf[total] = '\0';

    JsonDocument doc;
    // Cast const char* : force le mode copie, le document devient proprietaire de ses chaines.
    // Indispensable car profileManager->fromJson() lit des chaines du document apres le free().
    DeserializationError error = deserializeJson(doc, (const char*)buf);
    free(buf);
    request->_tempObject = nullptr;

    if (error) { request->send(400, "text/plain", "Invalid JSON"); return; }
    profileManager->fromJson(doc.as<JsonObjectConst>());
    configStore->saveProfile(*profileManager);
    request->send(200, "text/plain", "Profile saved");
}

void WebServerManager::handleProfileActivate(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    // Tampon par requete via _tempObject (corrige la concurrence du static String)
    // malloc/free : le destructeur de AsyncWebServerRequest libere _tempObject avec free()
    if (index == 0) {
        if (request->_tempObject != nullptr) {
            free(request->_tempObject);
            request->_tempObject = nullptr;
        }
        request->_tempObject = malloc(total + 1);
        if (!request->_tempObject) {
            request->send(500, "text/plain", "Allocation failed");
            return;
        }
    }

    char* buf = (char*)request->_tempObject;
    if (!buf) {
        request->send(500, "text/plain", "Internal error");
        return;
    }
    memcpy(buf + index, data, len);

    if (index + len != total) return;
    buf[total] = '\0';

    JsonDocument doc;
    // Cast const char* : force le mode copie, le document devient proprietaire de ses chaines
    DeserializationError error = deserializeJson(doc, (const char*)buf);
    free(buf);
    request->_tempObject = nullptr;

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
    // Tampon par requete via _tempObject (corrige la concurrence du static String)
    // malloc/free : le destructeur de AsyncWebServerRequest libere _tempObject avec free()
    if (index == 0) {
        if (request->_tempObject != nullptr) {
            free(request->_tempObject);
            request->_tempObject = nullptr;
        }
        request->_tempObject = malloc(total + 1);
        if (!request->_tempObject) {
            request->send(500, "application/json", "{\"error\":\"Allocation failed\"}");
            return;
        }
    }

    char* buf = (char*)request->_tempObject;
    if (!buf) {
        request->send(500, "application/json", "{\"error\":\"Internal error\"}");
        return;
    }
    memcpy(buf + index, data, len);

    if (index + len != total) return;
    buf[total] = '\0';

    JsonDocument doc;
    // Cast const char* : force le mode copie, le document devient proprietaire de ses chaines.
    // Indispensable car doc["stageName"].as<String>() et doc["action"].as<String>()
    // lisent des chaines du document apres le free().
    DeserializationError error = deserializeJson(doc, (const char*)buf);
    free(buf);
    request->_tempObject = nullptr;

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

void WebServerManager::handleRestart(AsyncWebServerRequest* request) {
    // Repondre AVANT d'armer le redemarrage : ESP.restart() dans le contexte
    // async tuerait la tache TCP avant l'envoi de la reponse
    request->send(200, "application/json", "{\"status\":\"restarting\",\"delay_ms\":1000}");
    _restartRequested = true;
    _restartDeadline = millis() + 1000;
}