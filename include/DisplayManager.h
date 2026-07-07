#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include "LGFX_Config.hpp" 
#include "Config.h"

struct DisplayData {
  float currentTemp;
  float setpoint;
  bool  coolOn;
  bool  heatOn;
  float gravity;
  float gravityStart;
  float angle;
  uint8_t battery;
  bool  iSpindelOnline;
  uint32_t iSpindelLastSeenMin;
  bool  mqttConnected;
  uint16_t fermentDays;
  String stageName;
  String profileStepLabel;
  uint8_t profileStepIndex;
  uint8_t profileStepCount;
  String ip;
  bool  fault;
};

class DisplayManager {
public:
  DisplayManager();
  void begin();
  void setBacklight(uint8_t percent);
  void update(const DisplayData& data);
  void forceRedraw();

private:
  LGFX tft;
  DisplayData lastData;
  unsigned long lastUpdateTime;

  // Méthodes privées pour dessiner les éléments
  void drawTopBar(const DisplayData& data);
  void drawLeftPanel(const DisplayData& data);
  void drawRightPanel(const DisplayData& data);
  void drawProfileBar(const DisplayData& data);
  void drawConnectionBar(const DisplayData& data);
  void drawFooter(const DisplayData& data);
  void drawBatteryIcon(uint8_t batteryLevel);
  void drawStatusChip(const String& label, bool active, uint16_t x, uint16_t y, uint16_t colorActive, uint16_t colorInactive);
  void drawStatusDot(bool connected, uint16_t x, uint16_t y);
  void drawProgressBar(uint8_t current, uint8_t total, uint16_t x, uint16_t y, uint16_t width);

  // Constantes de couleurs
  static const uint16_t COLOR_BACKGROUND;
  static const uint16_t COLOR_TEXT;
  static const uint16_t COLOR_ORANGE;
  static const uint16_t COLOR_YELLOW;
  static const uint16_t COLOR_BLUE;
  static const uint16_t COLOR_RED;
  static const uint16_t COLOR_GREEN;
  static const uint16_t COLOR_GRAY;
  static const uint16_t COLOR_ALERT;
};

#endif // DISPLAY_MANAGER_H
