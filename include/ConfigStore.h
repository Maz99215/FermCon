#ifndef CONFIGSTORE_H
#define CONFIGSTORE_H

#include <LittleFS.h>
#include <ArduinoJson.h>
#include "ProfileManager.h"
#include "FermentationInfo.h"
#include "Config.h"

struct SystemConfig {
    char wifi_ssid[33];
    char wifi_password[65];
    float setpoint;
    float hysteresis;
    uint32_t min_compressor_delay;
    float temp_offset;
    char username[32];
    char password_hash[64];   // mot de passe en clair (HTTP Basic), nom conserve pour compat
    bool mqtt_enabled;
    char mqtt_broker[64];
    int  mqtt_port;
    char mqtt_user[32];
    char mqtt_password[64];
    char mqtt_topic_prefix[64];
    char mqtt_device_name[32];
    bool gf_enabled;
    char gf_endpoint[128];
    char gf_device_label[40];
    bool ap_enabled;
    char ap_ssid[33];
    char ap_password[65];
};

struct SystemStatus {
    float temperature;
    float setpoint;
    bool  relay_fridge;
    bool  relay_heater;
    uint32_t uptime;
    int   wifi_rssi;
    uint32_t heap_free_kb;
    bool  temp_sensor_ok;
    char  ip_address[16];
    float isp_temperature;
    float isp_gravity;
    float isp_battery;
    float isp_angle;
    uint32_t isp_last_update;
    bool sta_connected;
    char ip_sta[16];
    char ip_ap[16];
    uint8_t ap_clients;
};

class ConfigStore {
public:
    ConfigStore();

    SystemConfig& getConfig();
    bool saveConfig(const SystemConfig& c);
    bool save();   // /config.json
    bool load();   // /config.json (+ defauts surs)

    // Persistance profil de temperature (/profile.json)
    bool saveProfile(const ProfileManager& profile);
    bool loadProfile(ProfileManager& profile);

    // Persistance metadonnees fermentation (/fermentation.json)
    bool saveFermentation(const FermentationInfo& info);
    bool loadFermentation(FermentationInfo& info);

    // Validation mot de passe point d'acces
    bool isApPasswordValid() const;

private:
    SystemConfig _config;
};

#endif // CONFIGSTORE_H
