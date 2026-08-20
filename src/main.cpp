#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <esp_task_wdt.h>
#include "ConfigStore.h"
#include "RelayController.h"
#include "TemperatureController.h"
#include "ISpindelReceiver.h"
#include "DataPublisher.h"
#include "ProfileManager.h"
#include "DisplayManager.h"
#include "WebServerManager.h"
#include <time.h>

// Masque des coeurs idle surveilles par le Task WDT
#if CONFIG_FREERTOS_UNICORE
  #define TWDT_IDLE_CORE_MASK 0x01
#else
  #define TWDT_IDLE_CORE_MASK 0x03
#endif

// Instances globales
ConfigStore configStore;
RelayController relays;
TemperatureController tempCtrl;
ISpindelReceiver ispindel;
DataPublisher publisher;
ProfileManager profile;
DisplayManager display;
SystemStatus systemStatus;
// v0.4.0 : FermentationInfo supprime, RelayController* retire du constructeur (BE7)
WebServerManager webServer(&configStore, &tempCtrl, &ispindel, &systemStatus, &profile, &publisher);

float gravityStart = 0.0f;
unsigned long lastMqttPublish = 0;

// ---------------------------------------------------------------------------
// NTP
// ---------------------------------------------------------------------------
bool timeIsValid() {
  return time(nullptr) > (time_t)NTP_VALID_EPOCH_MIN;
}

void ntpTask() {
  static unsigned long lastCheck = 0;
  static bool wasSynced = false;
  static bool wasConn = false;
  bool connected = (WiFi.status() == WL_CONNECTED);
  if (!connected) {
    wasSynced = false;
    wasConn = false;
    return;
  }
  if (!wasConn) {
    wasSynced = false;
    wasConn = true;
  }
  if (wasSynced) return;
  if (millis() - lastCheck < NTP_LOG_INTERVAL_MS) return;
  lastCheck = millis();
  if (timeIsValid()) {
    wasSynced = true;
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    Serial.print("[NTP] heure synchronisee: ");
    Serial.println(buf);
  }
}

// ---------------------------------------------------------------------------
// wifiTask
// ---------------------------------------------------------------------------
void wifiTask() {
  static unsigned long lastAttempt = 0;
  static bool wasConnected = false;
  SystemConfig& cfg = configStore.getConfig();
  bool connected = (WiFi.status() == WL_CONNECTED);
  if (connected) {
    if (!wasConnected) {
      Serial.print("[WIFI] STA connecte - IP: ");
      Serial.println(WiFi.localIP());
      wasConnected = true;
      configTzTime(NTP_TZ, NTP_SERVER_1, NTP_SERVER_2);
      Serial.println("[NTP] configTzTime lance");
    }
  } else {
    if (wasConnected) {
      Serial.println("[WIFI] STA deconnecte");
      wasConnected = false;
    }
    if (strlen(cfg.wifi_ssid) > 0 && millis() - lastAttempt >= WIFI_STA_RETRY_INTERVAL_MS) {
      Serial.print("[WIFI] Tentative reconnexion STA - SSID: ");
      Serial.println(cfg.wifi_ssid);
      WiFi.disconnect(false, false);
      WiFi.begin(cfg.wifi_ssid, cfg.wifi_password);
      lastAttempt = millis();
    }
  }
}

// ---------------------------------------------------------------------------
// setup
// ---------------------------------------------------------------------------
void setup() {
  relays.begin();

  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 3000) delay(10);
  Serial.println("\n[BOOT] setup entry");

  // Task WDT
  esp_task_wdt_config_t twdt_cfg = {
    .timeout_ms     = (uint32_t)(WDT_TIMEOUT_S * 1000),
    .idle_core_mask = TWDT_IDLE_CORE_MASK,
    .trigger_panic  = true
  };
  esp_err_t wdtErr = esp_task_wdt_init(&twdt_cfg);
  if (wdtErr == ESP_OK) {
    Serial.printf("[WDT] initialise (%d s)\n", WDT_TIMEOUT_S);
  } else if (wdtErr == ESP_ERR_INVALID_STATE) {
    wdtErr = esp_task_wdt_reconfigure(&twdt_cfg);
    if (wdtErr == ESP_OK) {
      Serial.printf("[WDT] reconfigure (%d s)\n", WDT_TIMEOUT_S);
    } else {
      Serial.printf("[WDT] ERREUR reconfigure: 0x%x\n", (int)wdtErr);
    }
  } else {
    Serial.printf("[WDT] ERREUR init: 0x%x\n", (int)wdtErr);
  }

  if (!LittleFS.begin(false)) {
    Serial.println("[BOOT] LittleFS MOUNT FAILED - lancer 'pio run -e esp32-c6 -t uploadfs'");
  } else {
    Serial.println("[BOOT] LittleFS OK");
  }

  configStore.load();
  Serial.println("[BOOT] config loaded");

  // NOUVEAU v0.3.0 : begin avec pointeur SystemConfig
  tempCtrl.begin(&relays, configStore.getConfigPtr());
  display.begin();
  Serial.println("[BOOT] managers OK");

  // v0.4.0 : FermentationInfo supprime, migration /fermentation.json -> Lot
  configStore.loadProfile(profile);
  configStore.migrateFermentationToBatch(profile);

  // WiFi (AP+STA simultane permanent)
  SystemConfig& cfg = configStore.getConfig();
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);

  // Demarrage SoftAP
  if (!cfg.ap_enabled) {
    Serial.println("[WIFI] AP desactive par configuration");
  } else if (!configStore.isApPasswordValid()) {
    Serial.println("[WIFI] ERREUR: mot de passe AP trop court (< 8 caracteres), AP non demarre pour securite");
  } else {
    if (WiFi.softAP(cfg.ap_ssid, cfg.ap_password, 1, 0, AP_MAX_CLIENTS)) {
      Serial.print("[WIFI] AP demarre - SSID: ");
      Serial.print(cfg.ap_ssid);
      Serial.print(" IP: ");
      Serial.println(WiFi.softAPIP());
    } else {
      Serial.println("[WIFI] ERREUR: echec demarrage AP");
    }
  }

  // Tentative STA non bloquante
  if (strlen(cfg.wifi_ssid) > 0) {
    WiFi.begin(cfg.wifi_ssid, cfg.wifi_password);
  }

  if (cfg.mqtt_enabled) {
    publisher.beginMqtt(cfg.mqtt_broker, cfg.mqtt_port, cfg.mqtt_user, cfg.mqtt_password, cfg.mqtt_topic_prefix, cfg.mqtt_device_name);
    publisher.setMqttEnabled(true);
  }

  if (cfg.gf_enabled) {
    publisher.configureGrainfather(cfg.gf_endpoint, cfg.gf_device_label);
    publisher.setGrainfatherEnabled(true);
  }

  webServer.begin();

  // Souscription Task WDT
  wdtErr = esp_task_wdt_add(NULL);
  if (wdtErr == ESP_OK) {
    Serial.println("[WDT] tache loop surveillee");
  } else {
    Serial.printf("[WDT] ERREUR add: 0x%x\n", (int)wdtErr);
  }
  esp_task_wdt_reset();
}

// ---------------------------------------------------------------------------
// loop
// ---------------------------------------------------------------------------
void loop() {
  // Regulation prioritaire
  tempCtrl.update();

  // v0.4.0 (ADR-011) : la consigne est pilotee par le lot uniquement si
  // active ET stepCount > 0. Sinon, SystemConfig.setpoint (consigne manuelle).
  float setpoint = profile.drivesSetpoint()
    ? profile.getCurrentSetpoint()
    : configStore.getConfig().setpoint;
  tempCtrl.setSetpoint(setpoint);

  // Sentinelle keep-alive
  static bool keepAliveLogged = false;
  if (relays.checkKeepAlive()) {
    if (!keepAliveLogged) {
      Serial.println("[SAFETY] sentinelle keep-alive expiree - sorties coupees");
      keepAliveLogged = true;
    }
  } else {
    keepAliveLogged = false;
  }

  bool staUp = (WiFi.status() == WL_CONNECTED);

  // Redistribution iSpindel
  if (ispindel.hasNewData()) {
    float t = ispindel.getTemperature();
    float g = ispindel.getGravity();
    float a = ispindel.getAngle();
    float b = ispindel.getBattery();
    if (staUp) {
      publisher.publishISpindel(t, g, a, b);
      if (configStore.getConfig().gf_enabled) {
        // NOUVEAU v0.3.0 : requestGrainfatherSend au lieu de sendToGrainfather
        publisher.requestGrainfatherSend(ispindel.getName().c_str(), ispindel.getID().c_str(),
                                          t, "C", g, a, b, ispindel.getRSSI());
      }
    }
    ispindel.clearNewData();
    if (g > 1.0f && gravityStart == 0.0f) gravityStart = g;
  }

  // NOUVEAU v0.3.0 : loopGrainfather non bloquant
  if (staUp) {
    publisher.loopGrainfather();
  }

  // Temperature cuve vers MQTT (30 s)
  if (staUp && millis() - lastMqttPublish >= 30000) {
    publisher.publishFermenterTemp(tempCtrl.getCurrentTemp());
    lastMqttPublish = millis();
  }

  if (staUp) {
    publisher.loop();
  }

  webServer.loop();
  wifiTask();
  ntpTask();

  // -----------------------------------------------------------------------
  // SystemStatus complet (etendu v0.3.0)
  // -----------------------------------------------------------------------
  systemStatus.temperature   = tempCtrl.getCurrentTemp();
  systemStatus.setpoint      = setpoint;
  systemStatus.state         = (uint8_t)tempCtrl.getState();
  systemStatus.relay_fridge  = relays.isCoolOn();
  systemStatus.relay_heater  = relays.isHeatOn();
  systemStatus.temp_sensor_ok = !tempCtrl.isFault();
  systemStatus.fault_count   = tempCtrl.getFaultCount();
  systemStatus.last_fault_epoch = tempCtrl.getLastFaultEpoch();
  systemStatus.last_rejected_reading = tempCtrl.getLastRejectedReading();
  systemStatus.fault_pending = tempCtrl.isFaultPending();
  systemStatus.has_valid_reading = tempCtrl.hasValidReading();

  systemStatus.uptime        = millis() / 1000;
  systemStatus.heap_free_kb  = ESP.getFreeHeap() / 1024;
  systemStatus.time_valid    = timeIsValid();

  systemStatus.sta_connected = staUp;
  systemStatus.wifi_rssi     = staUp ? WiFi.RSSI() : 0;
  systemStatus.mqtt_connected = publisher.isMqttConnected();

  strlcpy(systemStatus.ip_sta, staUp ? WiFi.localIP().toString().c_str() : "0.0.0.0", sizeof(systemStatus.ip_sta));
  strlcpy(systemStatus.ip_ap,  WiFi.softAPIP().toString().c_str(), sizeof(systemStatus.ip_ap));
  systemStatus.ap_clients    = (uint8_t)WiFi.softAPgetStationNum();
  strlcpy(systemStatus.ip_address, staUp ? WiFi.localIP().toString().c_str() : WiFi.softAPIP().toString().c_str(), sizeof(systemStatus.ip_address));

  // iSpindel
  systemStatus.isp_temperature = ispindel.getTemperature();
  systemStatus.isp_gravity     = ispindel.getGravity();
  systemStatus.isp_angle       = ispindel.getAngle();
  systemStatus.isp_battery     = ispindel.getBattery();
  systemStatus.isp_rssi        = ispindel.getRSSI();
  systemStatus.isp_last_update = ispindel.getLastUpdate();

  // isp_age_s : -1 si aucune donnee recue, sinon age en secondes
  if (ispindel.getLastUpdate() == 0) {
    systemStatus.isp_age_s = -1;
  } else {
    systemStatus.isp_age_s = (int32_t)((millis() - ispindel.getLastUpdate()) / 1000);
  }
  systemStatus.isp_online = (systemStatus.isp_age_s >= 0 && systemStatus.isp_age_s <= ISPINDEL_ONLINE_TIMEOUT_S);

  // Profil
  systemStatus.profile_active = profile.isActive();
  if (profile.isActive()) {
    String stepInfo = profile.getCurrentStepInfo();
    strlcpy(systemStatus.profile_step_label, stepInfo.c_str(), sizeof(systemStatus.profile_step_label));
    systemStatus.profile_step_index = profile.getCurrentStepIndex();
    systemStatus.profile_step_count = profile.getStepCount();
    systemStatus.profile_remaining_s = profile.getRemainingS();
  } else {
    strlcpy(systemStatus.profile_step_label, "Inactif", sizeof(systemStatus.profile_step_label));
    systemStatus.profile_step_index = 0;
    systemStatus.profile_step_count = 0;
    systemStatus.profile_remaining_s = -1;
  }

  // v0.4.0 : Fermentation alimentee depuis ProfileManager (entite Lot)
  systemStatus.ferment_days    = profile.getFermentDays();
  systemStatus.ferment_started = profile.isActive();
  strlcpy(systemStatus.stage_name, profile.getStageName().c_str(), sizeof(systemStatus.stage_name));

  // -----------------------------------------------------------------------
  // DisplayData (BE4 — char[] sans String)
  // v0.4.0 : champs fermentation alimentes depuis ProfileManager
  // -----------------------------------------------------------------------
  DisplayData d;
  d.currentTemp = tempCtrl.getCurrentTemp();
  d.setpoint    = setpoint;
  d.coolOn      = tempCtrl.isCoolOn();
  d.heatOn      = tempCtrl.isHeatOn();
  d.fault       = tempCtrl.isFault();
  d.faultCount  = tempCtrl.getFaultCount();

  d.gravity      = ispindel.getGravity();
  d.gravityStart = gravityStart;
  d.angle        = ispindel.getAngle();

  // CORRECTIF : batterie 255 si inconnue, JAMAIS de cast depuis NaN
  if (ispindel.getLastUpdate() == 0) {
    d.batteryPct = 255;
  } else {
    float bv = ispindel.getBattery();
    if (isnan(bv) || bv <= 0.0f) {
      d.batteryPct = 255;
    } else {
      long pct = map((long)(bv * 100), 330, 420, 0, 100);
      d.batteryPct = (uint8_t)constrain(pct, 0, 100);
    }
  }

  // ispindelOnline
  d.ispindelOnline = (ispindel.getLastUpdate() > 0 && (millis() - ispindel.getLastUpdate()) < (unsigned long)ISPINDEL_ONLINE_TIMEOUT_S * 1000UL);

  // ispindelAgeMin
  if (ispindel.getLastUpdate() == 0) {
    d.ispindelAgeMin = 65535;
  } else {
    unsigned long ageMin = (millis() - ispindel.getLastUpdate()) / 60000UL;
    d.ispindelAgeMin = (ageMin > 65534) ? 65535 : (uint16_t)ageMin;
  }

  d.ispindelRssi = ispindel.getRSSI();
  d.mqttConnected = publisher.isMqttConnected();
  d.wifiRssi     = staUp ? WiFi.RSSI() : 0;
  d.apClients    = (uint8_t)WiFi.softAPgetStationNum();

  // v0.4.0 : alimentes depuis ProfileManager
  d.fermentDays  = profile.getFermentDays();
  d.batchStarted = profile.isActive();
  strlcpy(d.stageName, profile.getStageName().c_str(), sizeof(d.stageName));

  // profileStepLabel : char[24], tronque
  if (profile.isActive()) {
    String stepInfo = profile.getCurrentStepInfo();
    strlcpy(d.profileStepLabel, stepInfo.c_str(), sizeof(d.profileStepLabel));
  } else {
    strlcpy(d.profileStepLabel, "Inactif", sizeof(d.profileStepLabel));
  }

  d.profileStepIndex = profile.isActive() ? profile.getCurrentStepIndex() : 0;
  d.profileStepCount = profile.isActive() ? profile.getStepCount() : 0;

  // profileRemainingH : heures restantes, -1 si inconnu
  if (profile.isActive()) {
    int32_t remS = profile.getRemainingS();
    d.profileRemainingH = (remS >= 0) ? (int16_t)(remS / 3600) : -1;
  } else {
    d.profileRemainingH = -1;
  }

  // ip : char[16], snprintf
  if (staUp) {
    strlcpy(d.ip, WiFi.localIP().toString().c_str(), sizeof(d.ip));
  } else {
    snprintf(d.ip, sizeof(d.ip), "AP %s", WiFi.softAPIP().toString().c_str());
  }

  // NOUVEAU v0.3.0 : DisplayManager::update() appele a chaque tour de boucle
  // (le throttle est gere dans DisplayManager, pas ici)
  display.update(d);

  esp_task_wdt_reset();
}
