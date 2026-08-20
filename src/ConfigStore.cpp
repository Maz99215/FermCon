#include "ConfigStore.h"
#include "ConfigValidator.h"

ConfigStore::ConfigStore() {
}

SystemConfig& ConfigStore::getConfig() {
    return _config;
}

const SystemConfig* ConfigStore::getConfigPtr() const {
    return &_config;
}

bool ConfigStore::saveConfig(const SystemConfig& c) {
    _config = c;
    return save();
}

// ---------------------------------------------------------------------------
// applyDefaults : valeurs par defaut de Config.h appliquees a toute la structure
// ---------------------------------------------------------------------------
void ConfigStore::applyDefaults(SystemConfig& cfg) {
    cfg.config_version = CONFIG_SCHEMA_VERSION;

    strlcpy(cfg.wifi_ssid, "", sizeof(cfg.wifi_ssid));
    strlcpy(cfg.wifi_password, "", sizeof(cfg.wifi_password));
    cfg.setpoint             = DEFAULT_SETPOINT_C;
    cfg.hysteresis           = TEMP_HYSTERESIS_C;
    cfg.min_compressor_delay = COMPRESSOR_MIN_OFF_S;
    cfg.temp_offset          = 0.0f;

    cfg.cool_min_on_s        = COOL_MIN_ON_S;
    cfg.heat_min_on_s        = HEAT_MIN_ON_S;
    cfg.max_on_timeout_s     = MAX_ON_TIMEOUT_S;
    cfg.temp_read_interval_ms = TEMP_READ_INTERVAL_MS;
    cfg.temp_plausible_min_c = TEMP_PLAUSIBLE_MIN_C;
    cfg.temp_plausible_max_c = TEMP_PLAUSIBLE_MAX_C;
    cfg.temp_fault_trip_s    = TEMP_FAULT_TRIP_S;
    cfg.temp_fault_clear_s   = TEMP_FAULT_CLEAR_S;

    strlcpy(cfg.username, "admin", sizeof(cfg.username));
    strlcpy(cfg.password_hash, "", sizeof(cfg.password_hash));

    cfg.mqtt_enabled = false;
    strlcpy(cfg.mqtt_broker, "", sizeof(cfg.mqtt_broker));
    cfg.mqtt_port = 1883;
    strlcpy(cfg.mqtt_user, "", sizeof(cfg.mqtt_user));
    strlcpy(cfg.mqtt_password, "", sizeof(cfg.mqtt_password));
    strlcpy(cfg.mqtt_topic_prefix, "fermcon", sizeof(cfg.mqtt_topic_prefix));
    strlcpy(cfg.mqtt_device_name, "fermcon", sizeof(cfg.mqtt_device_name));

    cfg.gf_enabled = false;
    strlcpy(cfg.gf_endpoint, "", sizeof(cfg.gf_endpoint));
    strlcpy(cfg.gf_device_label, "", sizeof(cfg.gf_device_label));

    cfg.ap_enabled = true;
    strlcpy(cfg.ap_ssid, DEFAULT_AP_SSID, sizeof(cfg.ap_ssid));
    strlcpy(cfg.ap_password, DEFAULT_AP_PASSWORD, sizeof(cfg.ap_password));
}

// ---------------------------------------------------------------------------
// loadFromFile : surcharge champ par champ depuis le JSON
// ---------------------------------------------------------------------------
void ConfigStore::loadFromFile(SystemConfig& cfg) {
    File file = LittleFS.open("/config.json", "r");
    if (!file) return;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error) return;

    // Champs existants v0.2.0
    if (!doc["wifi_ssid"].isNull())            strlcpy(cfg.wifi_ssid, doc["wifi_ssid"], sizeof(cfg.wifi_ssid));
    if (!doc["wifi_password"].isNull())        strlcpy(cfg.wifi_password, doc["wifi_password"], sizeof(cfg.wifi_password));
    if (!doc["setpoint"].isNull())             cfg.setpoint = doc["setpoint"];
    if (!doc["hysteresis"].isNull())           cfg.hysteresis = doc["hysteresis"];
    if (!doc["min_compressor_delay"].isNull()) cfg.min_compressor_delay = doc["min_compressor_delay"];
    if (!doc["temp_offset"].isNull())          cfg.temp_offset = doc["temp_offset"];
    if (!doc["username"].isNull())             strlcpy(cfg.username, doc["username"], sizeof(cfg.username));
    if (!doc["password_hash"].isNull())        strlcpy(cfg.password_hash, doc["password_hash"], sizeof(cfg.password_hash));
    if (!doc["mqtt_enabled"].isNull())         cfg.mqtt_enabled = doc["mqtt_enabled"];
    if (!doc["mqtt_broker"].isNull())          strlcpy(cfg.mqtt_broker, doc["mqtt_broker"], sizeof(cfg.mqtt_broker));
    if (!doc["mqtt_port"].isNull())            cfg.mqtt_port = doc["mqtt_port"];
    if (!doc["mqtt_user"].isNull())            strlcpy(cfg.mqtt_user, doc["mqtt_user"], sizeof(cfg.mqtt_user));
    if (!doc["mqtt_password"].isNull())        strlcpy(cfg.mqtt_password, doc["mqtt_password"], sizeof(cfg.mqtt_password));
    if (!doc["mqtt_topic_prefix"].isNull())    strlcpy(cfg.mqtt_topic_prefix, doc["mqtt_topic_prefix"], sizeof(cfg.mqtt_topic_prefix));
    if (!doc["mqtt_device_name"].isNull())     strlcpy(cfg.mqtt_device_name, doc["mqtt_device_name"], sizeof(cfg.mqtt_device_name));
    if (!doc["gf_enabled"].isNull())           cfg.gf_enabled = doc["gf_enabled"];
    if (!doc["gf_endpoint"].isNull())          strlcpy(cfg.gf_endpoint, doc["gf_endpoint"], sizeof(cfg.gf_endpoint));
    if (!doc["gf_device_label"].isNull())      strlcpy(cfg.gf_device_label, doc["gf_device_label"], sizeof(cfg.gf_device_label));
    if (!doc["ap_enabled"].isNull())           cfg.ap_enabled = doc["ap_enabled"];
    if (!doc["ap_ssid"].isNull())              strlcpy(cfg.ap_ssid, doc["ap_ssid"], sizeof(cfg.ap_ssid));
    if (!doc["ap_password"].isNull())          strlcpy(cfg.ap_password, doc["ap_password"], sizeof(cfg.ap_password));

    // NOUVEAU v0.3.0 — champs optionnels (absents dans un fichier v1)
    if (!doc["config_version"].isNull())        cfg.config_version = doc["config_version"];
    if (!doc["cool_min_on_s"].isNull())         cfg.cool_min_on_s = doc["cool_min_on_s"];
    if (!doc["heat_min_on_s"].isNull())         cfg.heat_min_on_s = doc["heat_min_on_s"];
    if (!doc["max_on_timeout_s"].isNull())      cfg.max_on_timeout_s = doc["max_on_timeout_s"];
    if (!doc["temp_read_interval_ms"].isNull()) cfg.temp_read_interval_ms = doc["temp_read_interval_ms"];
    if (!doc["temp_plausible_min_c"].isNull())  cfg.temp_plausible_min_c = doc["temp_plausible_min_c"];
    if (!doc["temp_plausible_max_c"].isNull())  cfg.temp_plausible_max_c = doc["temp_plausible_max_c"];
    if (!doc["temp_fault_trip_s"].isNull())     cfg.temp_fault_trip_s = doc["temp_fault_trip_s"];
    if (!doc["temp_fault_clear_s"].isNull())    cfg.temp_fault_clear_s = doc["temp_fault_clear_s"];
}

// ---------------------------------------------------------------------------
// migrateV1toV2 : un fichier sans config_version est traite comme v1
// ---------------------------------------------------------------------------
void ConfigStore::migrateV1toV2(SystemConfig& cfg) {
    // Les valeurs par defaut deja appliquees couvrent les 9 nouveaux champs.
    // On reecrit le fichier avec config_version = 2.
    cfg.config_version = CONFIG_SCHEMA_VERSION;
    Serial.println("[CONFIG] Migration v1 -> v2 : valeurs par defaut appliquees aux nouveaux champs");
}

// ---------------------------------------------------------------------------
// load : defauts surs + surcharge fichier + migration + ecretage
// ---------------------------------------------------------------------------
bool ConfigStore::load() {
    // 1) Appliquer les defauts surs a toute la structure
    applyDefaults(_config);

    // 2) Surcharger avec les valeurs du fichier si present
    if (LittleFS.exists("/config.json")) {
        loadFromFile(_config);

        // 3) Migration v1 -> v2 si le fichier n'a pas de config_version
        if (_config.config_version == 0) {
            migrateV1toV2(_config);
            // Sauvegarde immediate pour persister la migration
            save();
        }
    }

    // 4) Ecretage aux bornes dures (protection contre /config.json corrompu)
    clampConfig(_config);

    // Garde-fou : mot de passe AP trop court -> retour au defaut
    if (_config.ap_enabled && strlen(_config.ap_password) < AP_MIN_PASSWORD_LEN) {
        strlcpy(_config.ap_password, DEFAULT_AP_PASSWORD, sizeof(_config.ap_password));
        Serial.println("AVERTISSEMENT: mot de passe AP trop court (< 8 car.), retour au defaut");
    }

    return true;
}

// ---------------------------------------------------------------------------
// save : ecriture atomique via /config.tmp puis rename
// ---------------------------------------------------------------------------
bool ConfigStore::save() {
    JsonDocument doc;
    doc["config_version"]        = _config.config_version;
    doc["wifi_ssid"]             = _config.wifi_ssid;
    doc["wifi_password"]         = _config.wifi_password;
    doc["setpoint"]              = _config.setpoint;
    doc["hysteresis"]            = _config.hysteresis;
    doc["min_compressor_delay"]  = _config.min_compressor_delay;
    doc["temp_offset"]           = _config.temp_offset;
    doc["cool_min_on_s"]         = _config.cool_min_on_s;
    doc["heat_min_on_s"]         = _config.heat_min_on_s;
    doc["max_on_timeout_s"]      = _config.max_on_timeout_s;
    doc["temp_read_interval_ms"] = _config.temp_read_interval_ms;
    doc["temp_plausible_min_c"]  = _config.temp_plausible_min_c;
    doc["temp_plausible_max_c"]  = _config.temp_plausible_max_c;
    doc["temp_fault_trip_s"]     = _config.temp_fault_trip_s;
    doc["temp_fault_clear_s"]    = _config.temp_fault_clear_s;
    doc["username"]              = _config.username;
    doc["password_hash"]         = _config.password_hash;
    doc["mqtt_enabled"]          = _config.mqtt_enabled;
    doc["mqtt_broker"]           = _config.mqtt_broker;
    doc["mqtt_port"]             = _config.mqtt_port;
    doc["mqtt_user"]             = _config.mqtt_user;
    doc["mqtt_password"]         = _config.mqtt_password;
    doc["mqtt_topic_prefix"]     = _config.mqtt_topic_prefix;
    doc["mqtt_device_name"]      = _config.mqtt_device_name;
    doc["gf_enabled"]            = _config.gf_enabled;
    doc["gf_endpoint"]           = _config.gf_endpoint;
    doc["gf_device_label"]       = _config.gf_device_label;
    doc["ap_enabled"]            = _config.ap_enabled;
    doc["ap_ssid"]               = _config.ap_ssid;
    doc["ap_password"]           = _config.ap_password;

    // Ecriture atomique : /config.tmp puis rename
    File file = LittleFS.open("/config.tmp", "w");
    if (!file) {
        Serial.println("Erreur ouverture /config.tmp");
        return false;
    }
    if (serializeJson(doc, file) == 0) {
        Serial.println("Erreur ecriture /config.tmp");
        file.close();
        LittleFS.remove("/config.tmp");
        return false;
    }
    file.close();

    // Renommage atomique
    if (!LittleFS.rename("/config.tmp", "/config.json")) {
        Serial.println("Erreur rename /config.tmp -> /config.json");
        LittleFS.remove("/config.tmp");
        return false;
    }

    return true;
}

bool ConfigStore::isApPasswordValid() const {
    return strlen(_config.ap_password) >= AP_MIN_PASSWORD_LEN;
}

// ===== Persistance du profil de temperature (/profile.json) =====
// v0.4.0 : ecriture atomique via /profile.tmp puis rename
bool ConfigStore::saveProfile(const ProfileManager& profile) {
    JsonDocument doc;
    JsonObject json = doc.to<JsonObject>();
    profile.toJson(json);

    File file = LittleFS.open("/profile.tmp", "w");
    if (!file) {
        Serial.println("Erreur ouverture /profile.tmp");
        return false;
    }
    if (serializeJson(doc, file) == 0) {
        Serial.println("Erreur ecriture /profile.tmp");
        file.close();
        LittleFS.remove("/profile.tmp");
        return false;
    }
    file.close();

    if (!LittleFS.rename("/profile.tmp", "/profile.json")) {
        Serial.println("Erreur rename /profile.tmp -> /profile.json");
        LittleFS.remove("/profile.tmp");
        return false;
    }

    return true;
}

bool ConfigStore::loadProfile(ProfileManager& profile) {
    if (!LittleFS.exists("/profile.json")) return false;
    File file = LittleFS.open("/profile.json", "r");
    if (!file) {
        Serial.println("Erreur ouverture profile.json");
        return false;
    }
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    if (error) {
        Serial.println("Erreur parsing profile.json");
        file.close();
        return false;
    }
    profile.fromJson(doc.as<JsonObjectConst>());
    file.close();
    return true;
}

// ===== NOUVEAU v0.4.0 — Migration /fermentation.json -> Lot =====
bool ConfigStore::migrateFermentationToBatch(ProfileManager& batch) {
    if (!LittleFS.exists("/fermentation.json")) {
        return true;  // rien a migrer, succes
    }

    Serial.println("[MIGRATION] /fermentation.json detecte, migration en cours...");

    File file = LittleFS.open("/fermentation.json", "r");
    if (!file) {
        Serial.println("[MIGRATION] ERREUR: impossible d'ouvrir /fermentation.json");
        // On tente de supprimer le fichier meme en cas d'erreur
        LittleFS.remove("/fermentation.json");
        return true;  // non bloquant
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.println("[MIGRATION] ERREUR: parsing /fermentation.json echoue");
        LittleFS.remove("/fermentation.json");
        return true;  // non bloquant
    }

    bool started = doc["started"] | false;
    uint32_t fermStartEpoch = doc["startEpoch"] | 0;

    // Regle 1 (ARCHITECTURE §5.5) : lot inactif ET sans etape, ET started == true
    if (!batch.isActive() && batch.getStepCount() == 0 && started) {
        batch.setStartEpoch(fermStartEpoch);
        batch.setActive(true);
        Serial.println("[MIGRATION] Lot importe depuis /fermentation.json : actif, le decompte des jours se poursuit");
    }
    // Regle 2 : tous les autres cas — aucun import, avertissement serie
    else {
        Serial.println("[MIGRATION] AVERTISSEMENT SERIEUX: /fermentation.json present mais le lot possede des etapes ou est deja actif.");
        Serial.println("[MIGRATION] Aucune donnee importee. Veuillez redemarrer votre lot depuis l'interface web.");
        if (batch.getStepCount() > 0) {
            Serial.println("[MIGRATION] Raison: le lot contient des etapes. L'import automatique est bloque pour ne pas changer la consigne sans action utilisateur.");
        }
        if (batch.isActive()) {
            Serial.println("[MIGRATION] Raison: le lot est deja actif.");
        }
    }

    // Regle 3 : suppression de /fermentation.json dans les deux cas
    if (!LittleFS.remove("/fermentation.json")) {
        Serial.println("[MIGRATION] AVERTISSEMENT: echec de suppression de /fermentation.json (non bloquant)");
    } else {
        Serial.println("[MIGRATION] /fermentation.json supprime");
    }

    // Reecriture de /profile.json en schema_version = 2 (idempotent)
    batch.setSchemaVersion(PROFILE_SCHEMA_VERSION);
    saveProfile(batch);

    return true;
}
