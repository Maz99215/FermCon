#include "ConfigStore.h"

ConfigStore::ConfigStore() {
}

SystemConfig& ConfigStore::getConfig() {
    return _config;
}

bool ConfigStore::saveConfig(const SystemConfig& c) {
    _config = c;
    return save();
}

bool ConfigStore::save() {
    JsonDocument doc;
    doc["wifi_ssid"]            = _config.wifi_ssid;
    doc["wifi_password"]        = _config.wifi_password;
    doc["setpoint"]            = _config.setpoint;
    doc["hysteresis"]          = _config.hysteresis;
    doc["min_compressor_delay"] = _config.min_compressor_delay;
    doc["temp_offset"]          = _config.temp_offset;
    doc["username"]            = _config.username;
    doc["password_hash"]        = _config.password_hash;
    doc["mqtt_enabled"]         = _config.mqtt_enabled;
    doc["mqtt_broker"]          = _config.mqtt_broker;
    doc["mqtt_port"]            = _config.mqtt_port;
    doc["mqtt_user"]            = _config.mqtt_user;
    doc["mqtt_password"]        = _config.mqtt_password;
    doc["mqtt_topic_prefix"]    = _config.mqtt_topic_prefix;
    doc["mqtt_device_name"]     = _config.mqtt_device_name;
    doc["gf_enabled"]           = _config.gf_enabled;
    doc["gf_endpoint"]          = _config.gf_endpoint;
    doc["gf_device_label"]      = _config.gf_device_label;

    File file = LittleFS.open("/config.json", "w");
    if (!file) return false;
    serializeJson(doc, file);
    file.close();
    return true;
}

bool ConfigStore::load() {
    // 1) Defauts surs
    strlcpy(_config.wifi_ssid, "", sizeof(_config.wifi_ssid));
    strlcpy(_config.wifi_password, "", sizeof(_config.wifi_password));
    _config.setpoint             = DEFAULT_SETPOINT_C;
    _config.hysteresis           = TEMP_HYSTERESIS_C;
    _config.min_compressor_delay = COMPRESSOR_MIN_OFF_S;
    _config.temp_offset          = 0.0f;
    strlcpy(_config.username, "admin", sizeof(_config.username));
    strlcpy(_config.password_hash, "", sizeof(_config.password_hash));
    _config.mqtt_enabled = false;
    strlcpy(_config.mqtt_broker, "", sizeof(_config.mqtt_broker));
    _config.mqtt_port = 1883;
    strlcpy(_config.mqtt_user, "", sizeof(_config.mqtt_user));
    strlcpy(_config.mqtt_password, "", sizeof(_config.mqtt_password));
    strlcpy(_config.mqtt_topic_prefix, "fermcon", sizeof(_config.mqtt_topic_prefix));
    strlcpy(_config.mqtt_device_name, "fermcon", sizeof(_config.mqtt_device_name));
    _config.gf_enabled = false;
    strlcpy(_config.gf_endpoint, "", sizeof(_config.gf_endpoint));
    strlcpy(_config.gf_device_label, "", sizeof(_config.gf_device_label));

    // 2) Ecrasement par /config.json si present
    File file = LittleFS.open("/config.json", "r");
    if (!file) return false;
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error) return false;

    if (!doc["wifi_ssid"].isNull())            strlcpy(_config.wifi_ssid, doc["wifi_ssid"], sizeof(_config.wifi_ssid));
    if (!doc["wifi_password"].isNull())        strlcpy(_config.wifi_password, doc["wifi_password"], sizeof(_config.wifi_password));
    if (!doc["setpoint"].isNull())             _config.setpoint = doc["setpoint"];
    if (!doc["hysteresis"].isNull())           _config.hysteresis = doc["hysteresis"];
    if (!doc["min_compressor_delay"].isNull()) _config.min_compressor_delay = doc["min_compressor_delay"];
    if (!doc["temp_offset"].isNull())          _config.temp_offset = doc["temp_offset"];
    if (!doc["username"].isNull())             strlcpy(_config.username, doc["username"], sizeof(_config.username));
    if (!doc["password_hash"].isNull())        strlcpy(_config.password_hash, doc["password_hash"], sizeof(_config.password_hash));
    if (!doc["mqtt_enabled"].isNull())         _config.mqtt_enabled = doc["mqtt_enabled"];
    if (!doc["mqtt_broker"].isNull())          strlcpy(_config.mqtt_broker, doc["mqtt_broker"], sizeof(_config.mqtt_broker));
    if (!doc["mqtt_port"].isNull())            _config.mqtt_port = doc["mqtt_port"];
    if (!doc["mqtt_user"].isNull())            strlcpy(_config.mqtt_user, doc["mqtt_user"], sizeof(_config.mqtt_user));
    if (!doc["mqtt_password"].isNull())        strlcpy(_config.mqtt_password, doc["mqtt_password"], sizeof(_config.mqtt_password));
    if (!doc["mqtt_topic_prefix"].isNull())    strlcpy(_config.mqtt_topic_prefix, doc["mqtt_topic_prefix"], sizeof(_config.mqtt_topic_prefix));
    if (!doc["mqtt_device_name"].isNull())     strlcpy(_config.mqtt_device_name, doc["mqtt_device_name"], sizeof(_config.mqtt_device_name));
    if (!doc["gf_enabled"].isNull())           _config.gf_enabled = doc["gf_enabled"];
    if (!doc["gf_endpoint"].isNull())          strlcpy(_config.gf_endpoint, doc["gf_endpoint"], sizeof(_config.gf_endpoint));
    if (!doc["gf_device_label"].isNull())      strlcpy(_config.gf_device_label, doc["gf_device_label"], sizeof(_config.gf_device_label));

    return true;
}

// ===== Persistance du profil de temperature (/profile.json) =====
bool ConfigStore::saveProfile(const ProfileManager& profile) {
    File file = LittleFS.open("/profile.json", "w");
    if (!file) {
        Serial.println("Erreur ouverture fichier profile.json");
        return false;
    }
    JsonDocument doc;
    JsonObject json = doc.to<JsonObject>();
    profile.toJson(json);
    if (serializeJson(doc, file) == 0) {
        Serial.println("Erreur ecriture profile.json");
        file.close();
        return false;
    }
    file.close();
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

// ===== Persistance des metadonnees de fermentation (/fermentation.json) =====
bool ConfigStore::saveFermentation(const FermentationInfo& info) {
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    info.toJson(obj);
    File file = LittleFS.open("/fermentation.json", "w");
    if (!file) {
        Serial.println("Erreur ouverture fermentation.json");
        return false;
    }
    serializeJson(doc, file);
    file.close();
    return true;
}

bool ConfigStore::loadFermentation(FermentationInfo& info) {
    if (!LittleFS.exists("/fermentation.json")) return false;
    File file = LittleFS.open("/fermentation.json", "r");
    if (!file) return false;
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error) {
        Serial.println("Erreur parsing fermentation.json");
        return false;
    }
    info.fromJson(doc.as<JsonObjectConst>());
    return true;
}
