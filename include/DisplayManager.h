#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include "LGFX_Config.hpp"
#include "Config.h"

/**
 * DisplayData — FIGE, aucune String.
 * Tous les champs textuels sont des char[] de taille fixe.
 */
struct DisplayData {
  float currentTemp;                // NaN si pas de mesure valide
  float setpoint;
  bool  coolOn;
  bool  heatOn;
  bool  fault;
  uint32_t faultCount;
  float gravity;                    // NaN si inconnu
  float gravityStart;               // NaN si inconnu
  float angle;                      // NaN si inconnu
  uint8_t batteryPct;               // 255 = inconnu (JAMAIS de cast depuis NaN)
  bool  ispindelOnline;
  uint16_t ispindelAgeMin;          // 65535 = jamais vu
  int16_t ispindelRssi;             // 0 = inconnu
  bool  mqttConnected;
  int16_t wifiRssi;                 // dBm
  uint8_t apClients;
  uint16_t fermentDays;
  bool  batchStarted;
  char  stageName[24];              // tronque, jamais de String
  char  profileStepLabel[24];       // tronque, jamais de String
  uint8_t profileStepIndex;
  uint8_t profileStepCount;
  int16_t profileRemainingH;        // -1 = inconnu ou profil inactif
  char  ip[16];                     // jamais de String
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
  void drawOutputBar();
  void drawVerticalBar(int x, float fraction, uint16_t color, const char* label);
  static float clampf(float value, float lo, float hi);
};

#endif // DISPLAY_MANAGER_H
