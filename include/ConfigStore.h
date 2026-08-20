#ifndef CONFIGSTORE_H
#define CONFIGSTORE_H

#include <LittleFS.h>
#include <ArduinoJson.h>
#include "ProfileManager.h"
#include "Config.h"

// ---------------------------------------------------------------------------
// SystemConfig — etendu v0.3.0 (+9 champs + config_version)
// ---------------------------------------------------------------------------
struct SystemConfig {
    uint16_t config_version;         // NOUVEAU v0.3.0, = CONFIG_SCHEMA_VERSION

    char wifi_ssid[33];
    char wifi_password[65];
    float setpoint;
    float hysteresis;
    uint32_t min_compressor_delay;
    float temp_offset;

    // NOUVEAU v0.3.0 — parametres de regulation bornes
    uint32_t cool_min_on_s;
    uint32_t heat_min_on_s;
    uint32_t max_on_timeout_s;
    uint32_t temp_read_interval_ms;
    float temp_plausible_min_c;
    float temp_plausible_max_c;
    uint32_t temp_fault_trip_s;
    uint32_t temp_fault_clear_s;

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

// ---------------------------------------------------------------------------
// SystemStatus — etendu v0.3.0 (tous les champs de StatusResponse)
// ---------------------------------------------------------------------------
struct SystemStatus {
    // Regulation
    float temperature;
    float setpoint;
    uint8_t state;               // TemperatureController::State (0=IDLE,1=COOLING,2=HEATING,3=FAULT)
    bool  relay_fridge;
    bool  relay_heater;
    bool  temp_sensor_ok;
    uint32_t fault_count;
    uint32_t last_fault_epoch;
    float last_rejected_reading;
    bool  fault_pending;
    bool  has_valid_reading;

    // Systeme
    uint32_t uptime;
    uint32_t heap_free_kb;
    bool  time_valid;

    // Reseau
    bool  sta_connected;
    char  ip_sta[16];
    char  ip_ap[16];
    uint8_t ap_clients;
    int   wifi_rssi;
    char  ip_address[16];        // DEPRECIE, conserve pour compatibilite
    bool  mqtt_connected;

    // iSpindel
    float isp_temperature;
    float isp_gravity;
    float isp_angle;
    float isp_battery;
    int   isp_rssi;
    int32_t isp_age_s;
    bool  isp_online;
    uint32_t isp_last_update;    // DEPRECIE, millis brut

    // Profil
    bool  profile_active;
    char  profile_step_label[48];
    uint8_t profile_step_index;
    uint8_t profile_step_count;
    int32_t profile_remaining_s;

    // Fermentation — v0.4.0 : alimentes depuis ProfileManager, plus depuis FermentationInfo
    uint16_t ferment_days;
    bool  ferment_started;
    char  stage_name[32];
};

// ---------------------------------------------------------------------------
// ConfigStore
// ---------------------------------------------------------------------------
class ConfigStore {
public:
    ConfigStore();

    SystemConfig& getConfig();
    const SystemConfig* getConfigPtr() const;   // NOUVEAU v0.3.0

    bool saveConfig(const SystemConfig& c);
    bool save();   // /config.json (ecriture atomique)
    bool load();   // /config.json (+ defauts surs + migration v1->v2 + clamp)

    // Persistance profil de temperature (/profile.json)
    bool saveProfile(const ProfileManager& profile);
    bool loadProfile(ProfileManager& profile);

    // NOUVEAU v0.4.0 — Migration ponctuelle /fermentation.json -> Lot
    // Appelee une seule fois au demarrage. Idempotente.
    // Supprime /fermentation.json et reecrit /profile.json en schema_version=2.
    bool migrateFermentationToBatch(ProfileManager& batch);

    // Validation mot de passe point d'acces
    bool isApPasswordValid() const;

private:
    SystemConfig _config;

    void applyDefaults(SystemConfig& cfg);
    void loadFromFile(SystemConfig& cfg);
    void migrateV1toV2(SystemConfig& cfg);
};

#endif // CONFIGSTORE_H
