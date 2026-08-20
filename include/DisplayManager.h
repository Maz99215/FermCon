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
  bool batchStarted;
  String stageName;
  String profileStepLabel;
  uint8_t profileStepIndex;
  uint8_t profileStepCount;
  String ip;
  bool  fault;
  int   wifiRssi;       // dBm
  int   iSpindelRssi;   // dBm
  uint8_t apClients;    // clients Wi-Fi connectes au point d'acces
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

  // Helpers de dessin (layout paysage 284x76)
  void drawOutputBar();                                                     // barre SORTIE (gauche)
  void drawVerticalBar(int x, float fraction, uint16_t color, const String& label); // barres etat (droite)
  static float clampf(float value, float lo, float hi);
};

#endif // DISPLAY_MANAGER_H
