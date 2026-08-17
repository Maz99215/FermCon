#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include "ConfigStore.h"
#include "RelayController.h"
#include "TemperatureController.h"
#include "ISpindelReceiver.h"
#include "DataPublisher.h"
#include "ProfileManager.h"
#include "FermentationInfo.h"
#include "DisplayManager.h"
#include "WebServerManager.h"
#include <time.h>

// Instances globales
ConfigStore configStore;
RelayController relays;
TemperatureController tempCtrl;
ISpindelReceiver ispindel;
DataPublisher publisher;
ProfileManager profile;
FermentationInfo fermentation;
DisplayManager display;
SystemStatus systemStatus;
WebServerManager webServer(&configStore, &ispindel, &tempCtrl, &relays, &systemStatus, &profile, &fermentation);

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
// wifiTask - machine a etats non bloquante pour la reconnexion STA
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
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 3000) delay(10);
  Serial.println("\n[BOOT] setup entry");

  if (!LittleFS.begin(false)) {
    Serial.println("[BOOT] LittleFS MOUNT FAILED - lancer 'pio run -e esp32-c6 -t uploadfs'");
  } else {
    Serial.println("[BOOT] LittleFS OK");
  }

  configStore.load();
  Serial.println("[BOOT] config loaded");

  relays.begin();
  tempCtrl.begin(&relays);
  display.begin();
  Serial.println("[BOOT] managers OK");
  fermentation.begin();
  configStore.loadProfile(profile);
  configStore.loadFermentation(fermentation);

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
}

// ---------------------------------------------------------------------------
// loop
// ---------------------------------------------------------------------------
void loop() {
  // Regulation prioritaire
  tempCtrl.update();

  float setpoint = profile.isActive() ? profile.getCurrentSetpoint() : configStore.getConfig().setpoint;
  tempCtrl.setSetpoint(setpoint);

  bool staUp = (WiFi.status() == WL_CONNECTED);

  // Redistribution iSpindel - la reception n'est jamais conditionnee
  if (ispindel.hasNewData()) {
    float t = ispindel.getTemperature();
    float g = ispindel.getGravity();
    float a = ispindel.getAngle();
    float b = ispindel.getBattery();

    if (staUp) {
      publisher.publishISpindel(t, g, a, b);
      if (configStore.getConfig().gf_enabled) {
        publisher.sendToGrainfather(ispindel.getName().c_str(), ispindel.getID().c_str(), t, "C", g, a, b, ispindel.getRSSI());
      }
    }

    ispindel.clearNewData();
    if (g > 1.0f && gravityStart == 0.0f) gravityStart = g;
  }

  // Temperature cuve vers MQTT (30 s) - conditionne a STA
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

  // Etat systeme
  systemStatus.temperature   = tempCtrl.getCurrentTemp();
  systemStatus.setpoint      = setpoint;
  systemStatus.relay_fridge  = relays.isCoolOn();
  systemStatus.relay_heater  = relays.isHeatOn();
  systemStatus.uptime        = millis() / 1000;
  systemStatus.sta_connected = staUp;
  systemStatus.wifi_rssi     = staUp ? WiFi.RSSI() : 0;
  systemStatus.heap_free_kb  = ESP.getFreeHeap() / 1024;
  systemStatus.temp_sensor_ok = !tempCtrl.isFault();
  strlcpy(systemStatus.ip_sta, staUp ? WiFi.localIP().toString().c_str() : "0.0.0.0", sizeof(systemStatus.ip_sta));
  strlcpy(systemStatus.ip_ap,  WiFi.softAPIP().toString().c_str(), sizeof(systemStatus.ip_ap));
  systemStatus.ap_clients    = (uint8_t)WiFi.softAPgetStationNum();
  // Compatibilite ascendante : ip_address = IP STA si connectee, sinon IP AP
  strlcpy(systemStatus.ip_address, staUp ? WiFi.localIP().toString().c_str() : WiFi.softAPIP().toString().c_str(), sizeof(systemStatus.ip_address));
  systemStatus.isp_temperature = ispindel.getTemperature();
  systemStatus.isp_gravity     = ispindel.getGravity();
  systemStatus.isp_battery     = ispindel.getBattery();
  systemStatus.isp_angle       = ispindel.getAngle();
  systemStatus.isp_last_update = ispindel.getLastUpdate();

  // Affichage (500 ms)
  static unsigned long lastDisp = 0;
  if (millis() - lastDisp >= 500) {
    unsigned long lastMin = ispindel.getLastUpdate() > 0 ? (millis() - ispindel.getLastUpdate()) / 60000UL : 0;
    bool ispOnline = ispindel.getLastUpdate() > 0 && (millis() - ispindel.getLastUpdate()) < 600000UL;
    uint8_t battPct = (uint8_t)constrain(map((long)(ispindel.getBattery() * 100), 330, 420, 0, 100), 0, 100);

    DisplayData d = {
      tempCtrl.getCurrentTemp(),
      setpoint,
      tempCtrl.isCoolOn(),
      tempCtrl.isHeatOn(),
      ispindel.getGravity(),
      gravityStart,
      ispindel.getAngle(),
      battPct,
      ispOnline,
      (uint32_t)lastMin,
      publisher.isMqttConnected(),
      fermentation.getFermentDays(),
      fermentation.getStageName(),
      profile.getCurrentStepInfo(),
      0,
      0,
      staUp ? WiFi.localIP().toString() : String("AP ") + WiFi.softAPIP().toString(),
      tempCtrl.isFault(),
      staUp ? WiFi.RSSI() : 0,
      ispindel.getRSSI(),
      systemStatus.ap_clients
    };
    display.update(d);
    lastDisp = millis();
  }
}