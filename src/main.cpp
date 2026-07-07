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
unsigned long lastWiFiReconnect = 0;

void setup() {
  Serial.begin(115200);
  LittleFS.begin(true);

  configStore.load();
  relays.begin();
  tempCtrl.begin(&relays);
  display.begin();
  fermentation.begin();
  configStore.loadProfile(profile);
  configStore.loadFermentation(fermentation);

  // WiFi (STA avec repli SoftAP)
  SystemConfig& cfg = configStore.getConfig();
  WiFi.mode(WIFI_STA);
  if (strlen(cfg.wifi_ssid) > 0) {
    WiFi.begin(cfg.wifi_ssid, cfg.wifi_password);
    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) {
      delay(500);
      Serial.print(".");
    }
  }
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("FermCon");
    Serial.print("\nAP FermCon - IP: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.print("\nWiFi OK - IP: ");
    Serial.println(WiFi.localIP());
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

void loop() {
  // Regulation prioritaire
  tempCtrl.update();

  float setpoint = profile.isActive() ? profile.getCurrentSetpoint() : configStore.getConfig().setpoint;
  tempCtrl.setSetpoint(setpoint);

  // Redistribution iSpindel
  if (ispindel.hasNewData()) {
    float t = ispindel.getTemperature();
    float g = ispindel.getGravity();
    float a = ispindel.getAngle();
    float b = ispindel.getBattery();
    publisher.publishISpindel(t, g, a, b);
    if (configStore.getConfig().gf_enabled) {
      publisher.sendToGrainfather(ispindel.getName().c_str(), ispindel.getID().c_str(), t, "C", g, a, b, ispindel.getRSSI());
    }
    ispindel.clearNewData();
    if (g > 1.0f && gravityStart == 0.0f) gravityStart = g;
  }

  // Temperature cuve vers MQTT (30 s)
  if (millis() - lastMqttPublish >= 30000) {
    publisher.publishFermenterTemp(tempCtrl.getCurrentTemp());
    lastMqttPublish = millis();
  }

  publisher.loop();
  webServer.loop();

  // Etat systeme
  systemStatus.temperature   = tempCtrl.getCurrentTemp();
  systemStatus.setpoint      = setpoint;
  systemStatus.relay_fridge  = relays.isCoolOn();
  systemStatus.relay_heater  = relays.isHeatOn();
  systemStatus.uptime        = millis() / 1000;
  systemStatus.wifi_rssi     = WiFi.RSSI();
  systemStatus.heap_free_kb  = ESP.getFreeHeap() / 1024;
  systemStatus.temp_sensor_ok = !tempCtrl.isFault();
  strlcpy(systemStatus.ip_address, WiFi.localIP().toString().c_str(), sizeof(systemStatus.ip_address));
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
      WiFi.localIP().toString(),
      tempCtrl.isFault()
    };
    display.update(d);
    lastDisp = millis();
  }

  // Reconnexion WiFi
  if (WiFi.status() != WL_CONNECTED && strlen(configStore.getConfig().wifi_ssid) > 0 && millis() - lastWiFiReconnect >= 30000) {
    WiFi.reconnect();
    lastWiFiReconnect = millis();
  }

#ifdef WOKWI_SIM
  static unsigned long lastSim = 0;
  if (millis() - lastSim >= 15000) {
    ispindel.parsePayload("{\"name\":\"Sim\",\"ID\":1,\"temperature\":21.5,\"temp_units\":\"C\",\"gravity\":1.050,\"angle\":45,\"battery\":3.7,\"RSSI\":-60}");
    lastSim = millis();
  }
#endif
}
