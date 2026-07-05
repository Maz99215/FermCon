#include "DisplayManager.h"

// Constantes de couleurs
const uint16_t DisplayManager::COLOR_BACKGROUND = TFT_BLACK;
const uint16_t DisplayManager::COLOR_TEXT = TFT_WHITE;
const uint16_t DisplayManager::COLOR_ORANGE = TFT_ORANGE;
const uint16_t DisplayManager::COLOR_YELLOW = TFT_YELLOW;
const uint16_t DisplayManager::COLOR_BLUE = TFT_BLUE;
const uint16_t DisplayManager::COLOR_RED = TFT_RED;
const uint16_t DisplayManager::COLOR_GREEN = TFT_GREEN;
const uint16_t DisplayManager::COLOR_GRAY = TFT_DARKGREY;
const uint16_t DisplayManager::COLOR_ALERT = TFT_RED;

DisplayManager::DisplayManager() : tft(), lastUpdateTime(0) {
  // Initialisation des données
  lastData = {
    .currentTemp = 0.0f,
    .setpoint = 0.0f,
    .coolOn = false,
    .heatOn = false,
    .gravity = 0.0f,
    .gravityStart = 0.0f,
    .angle = 0.0f,
    .battery = 0,
    .iSpindelOnline = false,
    .iSpindelLastSeenMin = 0,
    .mqttConnected = false,
    .fermentDays = 0,
    .stageName = "",
    .profileStepLabel = "",
    .profileStepIndex = 0,
    .profileStepCount = 0,
    .ip = "",
    .fault = false
  };
}

void DisplayManager::begin() {
  tft.init();
  tft.setRotation(1); // Mode paysage
  tft.fillScreen(COLOR_BACKGROUND);
  ledcAttach(PIN_TFT_BL, TFT_BL_PWM_FREQ, TFT_BL_PWM_RES);
  setBacklight(50); // Luminosité par défaut
}

void DisplayManager::setBacklight(uint8_t percent) {
  uint8_t duty = map(percent, 0, 100, 0, 255);
  ledcWrite(PIN_TFT_BL, duty);
}

void DisplayManager::update(const DisplayData& data) {
  unsigned long currentTime = millis();
  if (currentTime - lastUpdateTime < 500) {
    return; // Throttle
  }
  lastUpdateTime = currentTime;

  // Vérifier les changements et dessiner uniquement les zones modifiées
  if (memcmp(&lastData, &data, sizeof(DisplayData)) != 0) {
    if (data.fault) {
      tft.fillScreen(COLOR_ALERT);
      tft.setTextColor(TFT_WHITE, COLOR_ALERT);
      tft.setTextSize(2);
      tft.setCursor(10, 100);
      tft.print("ALERTE: PANNE SONDE");
      tft.setTextSize(4);
      tft.setCursor(100, 140);
      tft.print("--.-");
      tft.setTextSize(2);
      tft.setCursor(130, 150);
      tft.print("C");
    } else {
      drawTopBar(data);
      drawLeftPanel(data);
      drawRightPanel(data);
      drawProfileBar(data);
      drawConnectionBar(data);
      drawFooter(data);
    }
    lastData = data;
  }
}

void DisplayManager::forceRedraw() {
  lastUpdateTime = 0;
  update(lastData);
}

void DisplayManager::drawTopBar(const DisplayData& data) {
  tft.fillRect(0, 0, 320, 26, COLOR_BACKGROUND);
  tft.setTextColor(COLOR_ORANGE);
  tft.setTextSize(2);
  tft.setCursor(6, 4);
  tft.print("J" + String(data.fermentDays));
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(50, 4);
  tft.print(data.stageName);
  drawBatteryIcon(data.battery);
}

void DisplayManager::drawLeftPanel(const DisplayData& data) {
  tft.fillRoundRect(6, 32, 151, 118, 8, COLOR_BACKGROUND);
  tft.setTextColor(COLOR_GRAY);
  tft.setTextSize(1);
  tft.setCursor(12, 38);
  tft.print("TEMPERATURE");
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(4);
  tft.setCursor(12, 54);
  tft.print(data.currentTemp, 1);
  tft.setTextSize(2);
  tft.setCursor(100, 64);
  tft.print("C");
  tft.setTextColor(COLOR_YELLOW);
  tft.setTextSize(1);
  tft.setCursor(12, 90);
  tft.print("Consigne " + String(data.setpoint, 1) + " C");
  drawStatusChip("FROID", data.coolOn, 12, 110, COLOR_BLUE, COLOR_GRAY);
  drawStatusChip("CHAUD", data.heatOn, 82, 110, COLOR_RED, COLOR_GRAY);
}

void DisplayManager::drawRightPanel(const DisplayData& data) {
  tft.fillRoundRect(163, 32, 151, 118, 8, COLOR_BACKGROUND);
  tft.setTextColor(COLOR_GRAY);
  tft.setTextSize(1);
  tft.setCursor(169, 38);
  tft.print("DENSITE (SG)");
  tft.setTextColor(COLOR_GREEN);
  tft.setTextSize(4);
  tft.setCursor(169, 54);
  tft.print(data.gravity, 3);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(1);
  tft.setCursor(169, 90);
  tft.print("Depart " + String(data.gravityStart, 3));
  float attenuation = 0.0f;
  if (data.gravityStart > 1.0f) {
    attenuation = ((data.gravityStart - data.gravity) / (data.gravityStart - 1.0f)) * 100.0f;
  }
  tft.setCursor(169, 102);
  tft.print("Attenuation " + String(attenuation, 1) + "%");
  tft.setCursor(169, 114);
  tft.print("Angle " + String(data.angle, 1));
}

void DisplayManager::drawProfileBar(const DisplayData& data) {
  tft.fillRect(0, 156, 320, 26, COLOR_BACKGROUND);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(1);
  tft.setCursor(6, 160);
  tft.print("PROFIL");
  tft.setCursor(6, 172);
  tft.print(data.profileStepLabel);
  if (data.profileStepCount > 0) {
    drawProgressBar(data.profileStepIndex, data.profileStepCount, 6, 188, 308);
    tft.setCursor(140, 188);
    tft.print(String(data.profileStepIndex) + "/" + String(data.profileStepCount));
  } else {
    tft.setCursor(6, 188);
    tft.print("Consigne fixe");
  }
}

void DisplayManager::drawConnectionBar(const DisplayData& data) {
  tft.fillRect(0, 188, 320, 26, COLOR_BACKGROUND);
  drawStatusDot(data.iSpindelOnline, 6, 194);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(1);
  tft.setCursor(26, 194);
  tft.print("iSpindel vu il y a " + String(data.iSpindelLastSeenMin) + " min");
  drawStatusDot(data.mqttConnected, 6, 206);
  tft.setCursor(26, 206);
  tft.print("MQTT " + String(data.mqttConnected ? "connecte" : "deconnecte"));
}

void DisplayManager::drawFooter(const DisplayData& data) {
  tft.fillRect(0, 222, 320, 18, COLOR_BACKGROUND);
  tft.setTextColor(TFT_DARKGREY);
  tft.setTextSize(1);
  tft.setCursor(160 - (data.ip.length() * 3), 224);
  tft.print("IP " + data.ip);
}

void DisplayManager::drawBatteryIcon(uint8_t batteryLevel) {
  uint16_t x = 280;
  uint16_t y = 4;
  tft.drawRect(x, y, 20, 12, COLOR_TEXT);
  tft.fillRect(x + 20, y + 3, 2, 6, COLOR_TEXT);
  uint8_t fillWidth = map(batteryLevel, 0, 100, 0, 18);
  uint16_t fillColor = (batteryLevel > 50) ? COLOR_GREEN : (batteryLevel > 20) ? COLOR_YELLOW : COLOR_RED;
  tft.fillRect(x + 1, y + 1, fillWidth, 10, fillColor);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(1);
  tft.setCursor(x + 24, y + 2);
  tft.print(String(batteryLevel) + "%");
}

void DisplayManager::drawStatusChip(const String& label, bool active, uint16_t x, uint16_t y, uint16_t colorActive, uint16_t colorInactive) {
  uint16_t color = active ? colorActive : colorInactive;
  tft.fillRoundRect(x, y, 60, 20, 10, color);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(1);
  tft.setCursor(x + 10, y + 6);
  tft.print(label);
}

void DisplayManager::drawStatusDot(bool connected, uint16_t x, uint16_t y) {
  tft.fillCircle(x + 4, y + 4, 4, connected ? COLOR_GREEN : COLOR_RED);
}

void DisplayManager::drawProgressBar(uint8_t current, uint8_t total, uint16_t x, uint16_t y, uint16_t width) {
  tft.drawRoundRect(x, y, width, 10, 5, COLOR_TEXT);
  if (total > 0) {
    uint16_t fillWidth = map(current, 1, total, 0, width - 2);
    tft.fillRoundRect(x + 1, y + 1, fillWidth, 8, 4, COLOR_GREEN);
  }
}
