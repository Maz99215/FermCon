#include "DisplayManager.h"

// NB : les couleurs TFT_BLACK / TFT_WHITE / TFT_RED / ... proviennent des macros
// de compatibilite fournies par LovyanGFX (via LGFX_Config.hpp).

DisplayManager::DisplayManager() : lastUpdateTime(0) {}

void DisplayManager::begin() {
  tft.init();
  tft.setRotation(0);              // portrait bandeau : width()=76, height()=284
  tft.fillScreen(TFT_BLACK);
  ledcAttach(PIN_TFT_BL, TFT_BL_PWM_FREQ, TFT_BL_PWM_RES);
  setBacklight(70);
}

void DisplayManager::setBacklight(uint8_t percent) {
  ledcWrite(PIN_TFT_BL, map(percent, 0, 100, 0, 255));
}

void DisplayManager::update(const DisplayData& data) {
  unsigned long now = millis();
  if (now - lastUpdateTime < 500) return;   // throttle 500 ms
  lastUpdateTime = now;
  // Redraw complet a chaque cycle throttle : sur ~76x284 c'est rapide et evite
  // tout memcmp non defini sur une struct contenant des String.
  lastData = data;
  forceRedraw();
}

void DisplayManager::forceRedraw() {
  if (lastData.fault) {
    tft.fillScreen(TFT_RED);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("PANNE", tft.width() / 2, 110);
    tft.drawString("SONDE", tft.width() / 2, 140);
    tft.setTextSize(3);
    tft.drawString("--.-", tft.width() / 2, 180);
    return;
  }

  tft.fillScreen(TFT_BLACK);
  drawHeader();
  drawTempBlock();
  drawRelayChips();
  drawGravityBlock();
  drawProfileBlock();
  drawConnBlock();
  drawFooter();
}

void DisplayManager::drawHeader() {
  // Jours de fermentation
  tft.setTextColor(TFT_ORANGE);
  tft.setTextSize(2);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("J" + String(lastData.fermentDays), 2, 2);

  // Batterie iSpindel (haut droite)
  drawBatteryIcon(lastData.battery);

  // Nom d'etape (libelle libre, tronque)
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(lastData.stageName.substring(0, 12), 2, 22);

  tft.drawFastHLine(0, 34, tft.width(), TFT_DARKGREY);
}

void DisplayManager::drawTempBlock() {
  tft.setTextColor(TFT_DARKGREY);
  tft.setTextSize(1);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("TEMP", tft.width() / 2, 40);

  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(3);
  tft.setTextDatum(TC_DATUM);
  tft.drawFloat(lastData.currentTemp, 1, tft.width() / 2, 52);

  tft.setTextColor(TFT_YELLOW);
  tft.setTextSize(1);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("-> " + String(lastData.setpoint, 1) + "C", tft.width() / 2, 82);
}

void DisplayManager::drawRelayChips() {
  // Deux chips empiles horizontalement, largeur 34 chacun (2+34+2+34+2 <= 76)
  tft.fillRoundRect(3, 98, 34, 18, 4, lastData.coolOn ? TFT_BLUE : TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("FROID", 20, 107);

  tft.fillRoundRect(39, 98, 34, 18, 4, lastData.heatOn ? TFT_RED : TFT_DARKGREY);
  tft.drawString("CHAUD", 56, 107);
}

void DisplayManager::drawGravityBlock() {
  tft.drawFastHLine(0, 124, tft.width(), TFT_DARKGREY);
  tft.setTextColor(TFT_DARKGREY);
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("SG", 2, 130);

  tft.setTextColor(TFT_GREEN);
  tft.setTextSize(2);
  tft.drawFloat(lastData.gravity, 3, 2, 143);

  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.drawString("dep " + String(lastData.gravityStart, 3), 2, 165);

  if (lastData.gravityStart > 1.0f) {
    float att = (lastData.gravityStart - lastData.gravity) / (lastData.gravityStart - 1.0f) * 100.0f;
    tft.drawString("att " + String(att, 1) + "%", 2, 177);
  } else {
    tft.drawString("att --", 2, 177);
  }
  tft.drawString("ang " + String(lastData.angle, 1), 2, 189);
}

void DisplayManager::drawProfileBlock() {
  tft.drawFastHLine(0, 203, tft.width(), TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(lastData.profileStepLabel.substring(0, 12), 2, 208);

  if (lastData.profileStepCount > 0) {
    drawProgressBar(lastData.profileStepIndex, lastData.profileStepCount, 222);
  } else {
    tft.drawString("Consigne fixe", 2, 222);
  }
}

void DisplayManager::drawConnBlock() {
  tft.drawFastHLine(0, 236, tft.width(), TFT_DARKGREY);
  drawDot(lastData.iSpindelOnline, 244);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("iSp " + String(lastData.iSpindelLastSeenMin) + "min", 12, 241);

  drawDot(lastData.mqttConnected, 258);
  tft.drawString(lastData.mqttConnected ? "MQTT ok" : "MQTT --", 12, 255);
}

void DisplayManager::drawFooter() {
  tft.setTextColor(TFT_DARKGREY);
  tft.setTextSize(1);
  tft.setTextDatum(BC_DATUM);
  tft.drawString(lastData.ip, tft.width() / 2, tft.height() - 2);
}

void DisplayManager::drawBatteryIcon(uint8_t percent) {
  uint16_t color = (percent > 50) ? TFT_GREEN : (percent > 20) ? TFT_YELLOW : TFT_RED;
  int x = 48, y = 2;
  tft.drawRect(x, y, 20, 10, TFT_WHITE);
  tft.fillRect(x + 20, y + 3, 2, 4, TFT_WHITE);
  tft.fillRect(x + 1, y + 1, map(percent, 0, 100, 0, 18), 8, color);
}

void DisplayManager::drawDot(bool connected, int y) {
  tft.fillCircle(6, y + 3, 3, connected ? TFT_GREEN : TFT_RED);
}

void DisplayManager::drawProgressBar(uint8_t current, uint8_t total, int y) {
  int w = 72, x = 2, h = 6;
  tft.drawRect(x, y, w, h, TFT_WHITE);
  if (total > 0) {
    int pw = map(current, 0, total, 0, w - 2);
    tft.fillRect(x + 1, y + 1, pw, h - 2, TFT_GREEN);
  }
}
