#include "WebServerManager.h"
#include "ConfigValidator.h"
#include <time.h>

// ---------------------------------------------------------------------------
// Constructeur v0.4.0 — FermentationInfo* et RelayController* retires (BE7)
// Signature alignee sur CONTRACTS_v0.4.0 §1.7
// ---------------------------------------------------------------------------
WebServerManager::WebServerManager(ConfigStore* cfg, TemperatureController* temp,
                                   ISpindelReceiver* isp, SystemStatus* status,
                                   ProfileManager* profile,
                                   DataPublisher* publisher)
    : server(80), configStore(cfg), ispindelReceiver(isp),
      temperatureController(temp),
      systemStatus(status), profileManager(profile),
      dataPublisher(publisher) {}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
void WebServerManager::sendJson(AsyncWebServerRequest* request, int code, const JsonDocument& doc) {
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    response->setCode(code);
    response->addHeader("Cache-Control", "no-store");
    serializeJson(doc, *response);
    request->send(response);
}

void WebServerManager::sendError(AsyncWebServerRequest* request, int httpCode,
                                 const char* errorCode, const char* message,
                                 const char* field, float minVal, float maxVal) {
    JsonDocument doc;
    JsonObject err = doc["error"].to<JsonObject>();
    err["code"] = errorCode;
    err["message"] = message;
    if (field) err["field"] = field;
    if (minVal != 0 || maxVal != 0) {
        err["min"] = minVal;
        err["max"] = maxVal;
    }
    sendJson(request, httpCode, doc);
}

// ---------------------------------------------------------------------------
// Authentification HTTP Basic
// ---------------------------------------------------------------------------
bool WebServerManager::authenticate(AsyncWebServerRequest* request) {
    SystemConfig& cfg = configStore->getConfig();
    if (strlen(cfg.username) == 0) return true; // pas d'auth configuree

    if (!request->authenticate(cfg.username, cfg.password_hash)) {
        request->requestAuthentication();
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// begin : enregistrement des routes — v0.4.0 : /api/fermentation supprimees
// ---------------------------------------------------------------------------
void WebServerManager::begin() {
    // OTA (sans auth pour compatibilite ElegantOTA)
    ElegantOTA.begin(&server);

    // Routes statiques avec Cache-Control
    server.serveStatic("/", LittleFS, "/web/").setDefaultFile("index.html")
          .setCacheControl("max-age=600");

    // Routes API avec Cache-Control: no-store
    // ATTENTION : /api/profile/activate doit etre enregistre AVANT /api/profile
    // pour eviter la collision de route (regression v0.2.0)

    server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* r) {
        if (!authenticate(r)) return;
        handleStatus(r);
    });

    server.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* r) {
        if (!authenticate(r)) return;
        handleConfigGet(r);
    });

    server.on("/api/config", HTTP_POST,
        [this](AsyncWebServerRequest* r) { if (!authenticate(r)) return; },
        NULL,
        [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t) {
            handleConfigPost(r, d, l, i, t);
        });

    server.on("/api/setpoint", HTTP_POST,
        [this](AsyncWebServerRequest* r) { if (!authenticate(r)) return; },
        NULL,
        [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t) {
            handleSetpoint(r, d, l, i, t);
        });

    // /api/profile/activate AVANT /api/profile
    server.on("/api/profile/activate", HTTP_POST,
        [this](AsyncWebServerRequest* r) { if (!authenticate(r)) return; },
        NULL,
        [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t) {
            handleProfileActivate(r, d, l, i, t);
        });

    server.on("/api/profile", HTTP_GET, [this](AsyncWebServerRequest* r) {
        if (!authenticate(r)) return;
        handleProfileGet(r);
    });

    server.on("/api/profile", HTTP_POST,
        [this](AsyncWebServerRequest* r) { if (!authenticate(r)) return; },
        NULL,
        [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t) {
            handleProfilePost(r, d, l, i, t);
        });

    // /api/fermentation SUPPRIME en v0.4.0 — route 404 via onNotFound

    server.on("/api/restart", HTTP_POST, [this](AsyncWebServerRequest* r) {
        if (!authenticate(r)) return;
        handleRestart(r);
    });

    // iSpindel — sans authentification, reponses text/plain
    server.on("/ispindel", HTTP_POST,
        [this](AsyncWebServerRequest* r) {},
        NULL,
        [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t) {
            handleISpindel(r, d, l, i, t);
        });

    // Route 404
    server.onNotFound([this](AsyncWebServerRequest* r) {
        handleNotFound(r);
    });

    server.begin();
    Serial.println("[HTTP] serveur demarre");
}

void WebServerManager::loop() {
    ElegantOTA.loop();

    if (_restartRequested && millis() >= _restartDeadline) {
        ESP.restart();
    }
}

// ---------------------------------------------------------------------------
// handleNotFound : 404 JSON
// ---------------------------------------------------------------------------
void WebServerManager::handleNotFound(AsyncWebServerRequest* request) {
    sendError(request, 404, "NOT_FOUND", "Route inconnue");
}

// ---------------------------------------------------------------------------
// GET /api/status — enrichi v0.3.0, champs inchanges en v0.4.0
// ferment_days, ferment_started, stage_name proviennent de ProfileManager
// via SystemStatus (alimente par main.cpp BE6)
// ---------------------------------------------------------------------------
void WebServerManager::handleStatus(AsyncWebServerRequest* request) {
    JsonDocument doc;

    // Regulation
    if (!isnan(systemStatus->temperature)) doc["temperature"] = systemStatus->temperature;
    doc["setpoint"]            = systemStatus->setpoint;
    const char* stateStr = "IDLE";
    switch ((TemperatureController::State)systemStatus->state) {
        case TemperatureController::State::IDLE:    stateStr = "IDLE"; break;
        case TemperatureController::State::COOLING: stateStr = "COOLING"; break;
        case TemperatureController::State::HEATING: stateStr = "HEATING"; break;
        case TemperatureController::State::FAULT:   stateStr = "FAULT"; break;
    }
    doc["state"]               = stateStr;
    doc["relay_fridge"]        = systemStatus->relay_fridge;
    doc["relay_heater"]        = systemStatus->relay_heater;
    doc["temp_sensor_ok"]      = systemStatus->temp_sensor_ok;
    doc["fault_count"]         = systemStatus->fault_count;
    doc["last_fault_epoch"]    = systemStatus->last_fault_epoch;
    if (!isnan(systemStatus->last_rejected_reading)) doc["last_rejected_reading"] = systemStatus->last_rejected_reading;
    doc["fault_pending"]       = systemStatus->fault_pending;
    doc["has_valid_reading"]   = systemStatus->has_valid_reading;

    // Systeme
    doc["uptime"]              = systemStatus->uptime;
    doc["heap_free_kb"]        = systemStatus->heap_free_kb;
    doc["fw_version"]          = FW_VERSION;
    doc["config_version"]      = CONFIG_SCHEMA_VERSION;
    doc["time_valid"]          = systemStatus->time_valid;

    // Reseau
    doc["sta_connected"]       = systemStatus->sta_connected;
    doc["ip_sta"]              = systemStatus->ip_sta;
    doc["ip_ap"]               = systemStatus->ip_ap;
    doc["ap_clients"]          = systemStatus->ap_clients;
    doc["wifi_rssi"]           = systemStatus->wifi_rssi;
    doc["ip_address"]          = systemStatus->ip_address;   // DEPRECIE
    doc["mqtt_connected"]      = systemStatus->mqtt_connected;

    // iSpindel
    if (!isnan(systemStatus->isp_temperature)) doc["isp_temperature"] = systemStatus->isp_temperature;
    if (!isnan(systemStatus->isp_gravity)) doc["isp_gravity"] = systemStatus->isp_gravity;
    if (!isnan(systemStatus->isp_angle)) doc["isp_angle"] = systemStatus->isp_angle;
    if (!isnan(systemStatus->isp_battery)) doc["isp_battery"] = systemStatus->isp_battery;
    doc["isp_rssi"]            = systemStatus->isp_rssi;
    doc["isp_age_s"]           = systemStatus->isp_age_s;
    doc["isp_online"]          = systemStatus->isp_online;
    doc["isp_last_update"]     = systemStatus->isp_last_update;  // DEPRECIE

    // Profil
    doc["profile_active"]      = systemStatus->profile_active;
    doc["profile_step_label"]  = systemStatus->profile_step_label;
    doc["profile_step_index"]  = systemStatus->profile_step_index;
    doc["profile_step_count"]  = systemStatus->profile_step_count;
    doc["profile_remaining_s"] = systemStatus->profile_remaining_s;

    // Fermentation (source : ProfileManager via SystemStatus, BE6)
    doc["ferment_days"]        = systemStatus->ferment_days;
    doc["ferment_started"]     = systemStatus->ferment_started;
    doc["stage_name"]          = systemStatus->stage_name;

    sendJson(request, 200, doc);
}

// ---------------------------------------------------------------------------
// GET /api/config — avec bounds, mqtt_device_name, *_set
// ---------------------------------------------------------------------------
void WebServerManager::handleConfigGet(AsyncWebServerRequest* request) {
    SystemConfig& cfg = configStore->getConfig();
    JsonDocument doc;

    doc["config_version"]        = cfg.config_version;
    doc["wifi_ssid"]             = cfg.wifi_ssid;
    doc["wifi_password_set"]     = strlen(cfg.wifi_password) > 0;
    doc["ap_enabled"]            = cfg.ap_enabled;
    doc["ap_ssid"]               = cfg.ap_ssid;
    doc["ap_password_set"]       = strlen(cfg.ap_password) > 0;
    doc["setpoint"]              = cfg.setpoint;
    doc["hysteresis"]            = cfg.hysteresis;
    doc["temp_offset"]           = cfg.temp_offset;
    doc["min_compressor_delay"]  = cfg.min_compressor_delay;
    doc["cool_min_on_s"]         = cfg.cool_min_on_s;
    doc["heat_min_on_s"]         = cfg.heat_min_on_s;
    doc["max_on_timeout_s"]      = cfg.max_on_timeout_s;
    doc["temp_read_interval_ms"] = cfg.temp_read_interval_ms;
    doc["temp_plausible_min_c"]  = cfg.temp_plausible_min_c;
    doc["temp_plausible_max_c"]  = cfg.temp_plausible_max_c;
    doc["temp_fault_trip_s"]     = cfg.temp_fault_trip_s;
    doc["temp_fault_clear_s"]    = cfg.temp_fault_clear_s;
    doc["username"]              = cfg.username;
    doc["password_set"]          = strlen(cfg.password_hash) > 0;
    // password_hash JAMAIS renvoye
    doc["mqtt_enabled"]          = cfg.mqtt_enabled;
    doc["mqtt_broker"]           = cfg.mqtt_broker;
    doc["mqtt_port"]             = cfg.mqtt_port;
    doc["mqtt_user"]             = cfg.mqtt_user;
    doc["mqtt_password_set"]     = strlen(cfg.mqtt_password) > 0;
    doc["mqtt_topic_prefix"]     = cfg.mqtt_topic_prefix;
    doc["mqtt_device_name"]      = cfg.mqtt_device_name;   // NOUVEAU
    doc["gf_enabled"]            = cfg.gf_enabled;
    doc["gf_endpoint"]           = cfg.gf_endpoint;
    doc["gf_device_label"]       = cfg.gf_device_label;

    // bounds
    JsonObject bounds = doc["bounds"].to<JsonObject>();
    JsonObject b;

    b = bounds["setpoint"].to<JsonObject>();             b["min"] = SETPOINT_MIN; b["max"] = SETPOINT_MAX;
    b = bounds["hysteresis"].to<JsonObject>();           b["min"] = HYSTERESIS_MIN; b["max"] = HYSTERESIS_MAX;
    b = bounds["temp_offset"].to<JsonObject>();          b["min"] = TEMP_OFFSET_MIN; b["max"] = TEMP_OFFSET_MAX;
    b = bounds["min_compressor_delay"].to<JsonObject>(); b["min"] = (float)MIN_COMPRESSOR_DELAY_MIN; b["max"] = (float)MIN_COMPRESSOR_DELAY_MAX;
    b = bounds["cool_min_on_s"].to<JsonObject>();        b["min"] = (float)COOL_MIN_ON_S_MIN; b["max"] = (float)COOL_MIN_ON_S_MAX;
    b = bounds["heat_min_on_s"].to<JsonObject>();        b["min"] = (float)HEAT_MIN_ON_S_MIN; b["max"] = (float)HEAT_MIN_ON_S_MAX;
    b = bounds["max_on_timeout_s"].to<JsonObject>();     b["min"] = (float)MAX_ON_TIMEOUT_S_MIN; b["max"] = (float)MAX_ON_TIMEOUT_S_MAX;
    b = bounds["temp_read_interval_ms"].to<JsonObject>();b["min"] = (float)TEMP_READ_INTERVAL_MS_MIN; b["max"] = (float)TEMP_READ_INTERVAL_MS_MAX;
    b = bounds["temp_plausible_min_c"].to<JsonObject>(); b["min"] = TEMP_PLAUSIBLE_MIN_C_MIN; b["max"] = TEMP_PLAUSIBLE_MIN_C_MAX;
    b = bounds["temp_plausible_max_c"].to<JsonObject>(); b["min"] = TEMP_PLAUSIBLE_MAX_C_MIN; b["max"] = TEMP_PLAUSIBLE_MAX_C_MAX;
    b = bounds["temp_fault_trip_s"].to<JsonObject>();    b["min"] = (float)TEMP_FAULT_TRIP_S_MIN; b["max"] = (float)TEMP_FAULT_TRIP_S_MAX;
    b = bounds["temp_fault_clear_s"].to<JsonObject>();   b["min"] = (float)TEMP_FAULT_CLEAR_S_MIN; b["max"] = (float)TEMP_FAULT_CLEAR_S_MAX;
    b = bounds["mqtt_port"].to<JsonObject>();            b["min"] = (float)MQTT_PORT_MIN; b["max"] = (float)MQTT_PORT_MAX;

    sendJson(request, 200, doc);
}

// ---------------------------------------------------------------------------
// POST /api/config — valide via ConfigValidator
// ---------------------------------------------------------------------------
void WebServerManager::handleConfigPost(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    if (index > 0 || !data || len == 0) {
        sendError(request, 400, "INVALID_JSON", "Corps JSON invalide");
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (const char*)data, len);
    if (error) {
        sendError(request, 400, "INVALID_JSON", "Corps JSON invalide");
        return;
    }

    // Copie locale pour validation sans effet de bord
    SystemConfig cfg = configStore->getConfig();

    // Appliquer les champs presents (mise a jour partielle)
    if (!doc["wifi_ssid"].isNull())             strlcpy(cfg.wifi_ssid, doc["wifi_ssid"], sizeof(cfg.wifi_ssid));
    if (!doc["wifi_password"].isNull() && strlen(doc["wifi_password"] | "") > 0) strlcpy(cfg.wifi_password, doc["wifi_password"], sizeof(cfg.wifi_password));
    if (!doc["ap_enabled"].isNull())            cfg.ap_enabled = doc["ap_enabled"];
    if (!doc["ap_ssid"].isNull())               strlcpy(cfg.ap_ssid, doc["ap_ssid"], sizeof(cfg.ap_ssid));
    if (!doc["ap_password"].isNull() && strlen(doc["ap_password"] | "") > 0) strlcpy(cfg.ap_password, doc["ap_password"], sizeof(cfg.ap_password));
    if (!doc["setpoint"].isNull())              cfg.setpoint = doc["setpoint"];
    if (!doc["hysteresis"].isNull())            cfg.hysteresis = doc["hysteresis"];
    if (!doc["temp_offset"].isNull())           cfg.temp_offset = doc["temp_offset"];
    if (!doc["min_compressor_delay"].isNull())  cfg.min_compressor_delay = doc["min_compressor_delay"];
    if (!doc["cool_min_on_s"].isNull())         cfg.cool_min_on_s = doc["cool_min_on_s"];
    if (!doc["heat_min_on_s"].isNull())         cfg.heat_min_on_s = doc["heat_min_on_s"];
    if (!doc["max_on_timeout_s"].isNull())      cfg.max_on_timeout_s = doc["max_on_timeout_s"];
    if (!doc["temp_read_interval_ms"].isNull()) cfg.temp_read_interval_ms = doc["temp_read_interval_ms"];
    if (!doc["temp_plausible_min_c"].isNull())  cfg.temp_plausible_min_c = doc["temp_plausible_min_c"];
    if (!doc["temp_plausible_max_c"].isNull())  cfg.temp_plausible_max_c = doc["temp_plausible_max_c"];
    if (!doc["temp_fault_trip_s"].isNull())     cfg.temp_fault_trip_s = doc["temp_fault_trip_s"];
    if (!doc["temp_fault_clear_s"].isNull())    cfg.temp_fault_clear_s = doc["temp_fault_clear_s"];
    if (!doc["username"].isNull())              strlcpy(cfg.username, doc["username"], sizeof(cfg.username));
    if (!doc["password"].isNull() && strlen(doc["password"] | "") > 0) strlcpy(cfg.password_hash, doc["password"], sizeof(cfg.password_hash));
    if (!doc["mqtt_enabled"].isNull())          cfg.mqtt_enabled = doc["mqtt_enabled"];
    if (!doc["mqtt_broker"].isNull())           strlcpy(cfg.mqtt_broker, doc["mqtt_broker"], sizeof(cfg.mqtt_broker));
    if (!doc["mqtt_port"].isNull())             cfg.mqtt_port = doc["mqtt_port"];
    if (!doc["mqtt_user"].isNull())             strlcpy(cfg.mqtt_user, doc["mqtt_user"], sizeof(cfg.mqtt_user));
    if (!doc["mqtt_password"].isNull() && strlen(doc["mqtt_password"] | "") > 0) strlcpy(cfg.mqtt_password, doc["mqtt_password"], sizeof(cfg.mqtt_password));
    if (!doc["mqtt_topic_prefix"].isNull())     strlcpy(cfg.mqtt_topic_prefix, doc["mqtt_topic_prefix"], sizeof(cfg.mqtt_topic_prefix));
    if (!doc["mqtt_device_name"].isNull())      strlcpy(cfg.mqtt_device_name, doc["mqtt_device_name"], sizeof(cfg.mqtt_device_name));
    if (!doc["gf_enabled"].isNull())            cfg.gf_enabled = doc["gf_enabled"];
    if (!doc["gf_endpoint"].isNull())           strlcpy(cfg.gf_endpoint, doc["gf_endpoint"], sizeof(cfg.gf_endpoint));
    if (!doc["gf_device_label"].isNull())       strlcpy(cfg.gf_device_label, doc["gf_device_label"], sizeof(cfg.gf_device_label));

    // Validation
    ValidationResult vr;
    if (!validateConfig(cfg, vr)) {
        sendError(request, 400, vr.code, vr.message, vr.field, vr.min, vr.max);
        return;
    }

    // Sauvegarde
    bool apChanged = (strcmp(cfg.ap_ssid, configStore->getConfig().ap_ssid) != 0 ||
                      strcmp(cfg.ap_password, configStore->getConfig().ap_password) != 0 ||
                      cfg.ap_enabled != configStore->getConfig().ap_enabled);

    if (!configStore->saveConfig(cfg)) {
        sendError(request, 500, "STORAGE_ERROR", "Ecriture de la configuration impossible");
        return;
    }

    JsonDocument resp;
    resp["status"] = "success";
    resp["reboot_required"] = apChanged;
    sendJson(request, 200, resp);
}

// ---------------------------------------------------------------------------
// POST /api/setpoint — v0.4.0 : 409 PROFILE_ACTIVE uniquement si
// le lot est actif ET possede au moins une etape (drivesSetpoint)
// ---------------------------------------------------------------------------
void WebServerManager::handleSetpoint(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    if (index > 0 || !data || len == 0) {
        sendError(request, 400, "INVALID_JSON", "Corps JSON invalide");
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (const char*)data, len);
    if (error || doc["setpoint"].isNull()) {
        sendError(request, 400, "MISSING_FIELD", "champ setpoint requis", "setpoint");
        return;
    }

    // v0.4.0 : seuls les lots actifs avec etapes bloquent la consigne manuelle
    if (profileManager->drivesSetpoint()) {
        sendError(request, 409, "PROFILE_ACTIVE", "Un lot actif pilote la consigne");
        return;
    }

    float sp = doc["setpoint"];
    if (sp < SETPOINT_MIN || sp > SETPOINT_MAX) {
        sendError(request, 400, "VALIDATION_ERROR", "setpoint hors bornes", "setpoint", SETPOINT_MIN, SETPOINT_MAX);
        return;
    }

    SystemConfig& cfg = configStore->getConfig();
    cfg.setpoint = sp;
    if (!configStore->save()) {
        sendError(request, 500, "STORAGE_ERROR", "Ecriture de la configuration impossible");
        return;
    }

    JsonDocument resp;
    resp["status"] = "success";
    resp["setpoint"] = sp;
    sendJson(request, 200, resp);
}

// ---------------------------------------------------------------------------
// GET /api/profile — v0.4.0 : enrichi ferment_days, drives_setpoint
// ---------------------------------------------------------------------------
void WebServerManager::handleProfileGet(AsyncWebServerRequest* request) {
    JsonDocument doc;
    JsonObject json = doc.to<JsonObject>();
    profileManager->toJson(json);

    // Champs calcules
    json["currentStep"] = profileManager->getCurrentStepInfo();
    json["setpoint"] = profileManager->getCurrentSetpoint();
    json["time_reference_valid"] = profileManager->isTimeReferenceValid();
    json["remaining_s"] = profileManager->getRemainingS();

    // NOUVEAU v0.4.0 — valeurs derivees du Lot
    json["ferment_days"] = profileManager->getFermentDays();
    json["drives_setpoint"] = profileManager->drivesSetpoint();

    sendJson(request, 200, doc);
}

// ---------------------------------------------------------------------------
// POST /api/profile — v0.4.0 : label d'etape optionnel, valide via
// ConfigValidator::validateStepLabel()
// ---------------------------------------------------------------------------
void WebServerManager::handleProfilePost(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    if (index > 0 || !data || len == 0) {
        sendError(request, 400, "INVALID_JSON", "Corps JSON invalide");
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (const char*)data, len);
    if (error) {
        sendError(request, 400, "INVALID_JSON", "Corps JSON invalide");
        return;
    }

    if (doc["name"].isNull()) {
        sendError(request, 400, "MISSING_FIELD", "champ name requis", "name");
        return;
    }
    if (doc["steps"].isNull()) {
        sendError(request, 400, "MISSING_FIELD", "champ steps requis", "steps");
        return;
    }

    // Valider chaque etape
    JsonArrayConst stepsArray = doc["steps"];
    size_t n = stepsArray.size();
    if (n > 16) n = 16;

    for (size_t i = 0; i < n; i++) {
        JsonObjectConst step = stepsArray[i];
        ProfileStep ps;
        ps.type      = ProfileManager::parseStepType(step["type"]);
        ps.tempStart = step["tempStart"];
        ps.tempEnd   = step["tempEnd"];
        ps.durationS = step["durationS"];

        ValidationResult vr;
        if (!validateProfileStep(ps, (uint8_t)i, vr)) {
            sendError(request, 400, vr.code, vr.message, vr.field, vr.min, vr.max);
            return;
        }

        // v0.4.0 : validation du label (optionnel, 0..23 caracteres, sans controle)
        if (!step["label"].isNull()) {
            const char* label = step["label"];
            if (!validateStepLabel(label, (uint8_t)i, vr)) {
                sendError(request, 400, vr.code, vr.message, vr.field, vr.min, vr.max);
                return;
            }
        }
    }

    // Appliquer (ne modifie ni active ni startEpoch)
    profileManager->clearSteps();
    profileManager->setName(doc["name"] | "");
    for (size_t i = 0; i < n; i++) {
        JsonObjectConst step = stepsArray[i];
        ProfileStep ps;
        ps.type      = ProfileManager::parseStepType(step["type"]);
        ps.tempStart = step["tempStart"];
        ps.tempEnd   = step["tempEnd"];
        ps.durationS = step["durationS"];
        // v0.4.0 : copie du label
        strlcpy(ps.label, step["label"] | "", sizeof(ps.label));
        profileManager->addStep(ps);
    }

    if (!configStore->saveProfile(*profileManager)) {
        sendError(request, 500, "STORAGE_ERROR", "Ecriture du profil impossible");
        return;
    }

    JsonDocument resp;
    resp["status"] = "success";
    resp["step_count"] = n;
    sendJson(request, 200, resp);
}

// ---------------------------------------------------------------------------
// POST /api/profile/activate — v0.4.0 : activation sans etape autorisee,
// startEpoch et drives_setpoint dans la reponse
// ---------------------------------------------------------------------------
void WebServerManager::handleProfileActivate(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    if (index > 0 || !data || len == 0) {
        sendError(request, 400, "INVALID_JSON", "Corps JSON invalide");
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (const char*)data, len);
    if (error || doc["active"].isNull()) {
        sendError(request, 400, "MISSING_FIELD", "champ active requis", "active");
        return;
    }

    bool activate = doc["active"];
    if (activate) {
        // Verifier reference de temps
        if (time(nullptr) <= (time_t)NTP_VALID_EPOCH_MIN) {
            sendError(request, 409, "TIME_NOT_SYNCED", "Horloge non synchronisee");
            return;
        }

        // v0.4.0 : activation autorisee sans etape (ADR-011)
        profileManager->start();
    } else {
        profileManager->stop();
    }

    // Sauvegarder l'etat active
    configStore->saveProfile(*profileManager);

    JsonDocument resp;
    resp["status"] = "success";
    resp["active"] = profileManager->isActive();
    resp["setpoint"] = profileManager->getCurrentSetpoint();
    // NOUVEAU v0.4.0
    resp["startEpoch"] = profileManager->getStartEpoch();
    resp["drives_setpoint"] = profileManager->drivesSetpoint();
    sendJson(request, 200, resp);
}

// ---------------------------------------------------------------------------
// POST /api/restart
// ---------------------------------------------------------------------------
void WebServerManager::handleRestart(AsyncWebServerRequest* request) {
    JsonDocument doc;
    doc["status"] = "restarting";
    doc["delay_ms"] = 1000;
    sendJson(request, 200, doc);

    _restartRequested = true;
    _restartDeadline = millis() + 1000;
}

// ---------------------------------------------------------------------------
// POST /ispindel — sans auth, reponses text/plain
// ---------------------------------------------------------------------------
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
