#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include "LGFX_Config.hpp"   // fournit LGFX + les macros couleur TFT_* (compat TFT_eSPI)
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

  // Blocs de dessin (layout vertical bandeau 76x284)
  void drawHeader();
  void drawTempBlock();
  void drawRelayChips();
  void drawGravityBlock();
  void drawProfileBlock();
  void drawConnBlock();
  void drawFooter();

  // Helpers
  void drawBatteryIcon(uint8_t percent);
  void drawDot(bool connected, int y);
  void drawProgressBar(uint8_t current, uint8_t total, int y);
};

#endif // DISPLAY_MANAGER_H
