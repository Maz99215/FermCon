#include "DisplayManager.h"

// Palette (RGB -> lgfx::color565). Les macros TFT_* restent disponibles.
constexpr uint16_t FROID     = lgfx::color565(14, 165, 233);
constexpr uint16_t CHAUD     = lgfx::color565(249, 115, 22);
constexpr uint16_t REPOS_BG  = lgfx::color565(17, 24, 39);
constexpr uint16_t REPOS_FG  = lgfx::color565(100, 116, 139);
constexpr uint16_t REPOS_BRD = lgfx::color565(51, 65, 85);
constexpr uint16_t SG_VIOLET = lgfx::color565(167, 139, 250);
constexpr uint16_t SKY       = lgfx::color565(56, 189, 248);   // IP + accent header
constexpr uint16_t VERT      = lgfx::color565(34, 197, 94);
constexpr uint16_t ORANGE    = lgfx::color565(245, 158, 11);
constexpr uint16_t ROUGE     = lgfx::color565(239, 68, 68);
constexpr uint16_t MUTED     = lgfx::color565(148, 163, 184);
constexpr uint16_t TRACK     = lgfx::color565(31, 41, 55);

DisplayManager::DisplayManager() : lastUpdateTime(0) {}

void DisplayManager::begin() {
  bool ok = tft.init();
  Serial.printf("[DISP] init=%d  w=%d h=%d\n", (int)ok, tft.width(), tft.height());

  tft.setRotation(3);              // paysage inverse : width()=284, height()=76
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
  // Redraw complet a chaque cycle : evite tout memcmp sur une struct a String.
  lastData = data;
  forceRedraw();
}

void DisplayManager::forceRedraw() {
  // --- Ecran de panne sonde ---
  if (lastData.fault) {
    tft.fillScreen(TFT_RED);
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.drawString("PANNE SONDE", 142, 28);
    tft.setTextSize(3);
    tft.drawString("--.-", 142, 54);
    return;
  }

  // Fond
  tft.fillScreen(TFT_BLACK);

  // 1) Barre SORTIE (gauche)
  drawOutputBar();

  // 2) Bloc central : separateur vertical
  tft.drawFastVLine(124, 6, 58, TRACK);

  // 2a) Colonne temperature
  // Header : Jxx (cyan) + nom d'etape (muted)
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(SKY);
  String jstr = "J" + String(lastData.fermentDays);
  tft.drawString(jstr, 22, 2);
  tft.setTextColor(MUTED);
  tft.drawString(lastData.stageName.substring(0, 10), 22 + tft.textWidth(jstr) + 3, 2);

  // Temperature (dominante) + unite C, centre sur x=72
  tft.setTextSize(3);
  String tstr = String(lastData.currentTemp, 1);
  int tW = tft.textWidth(tstr);
  tft.setTextSize(2);
  int uW = tft.textWidth("C");
  int startX = 72 - (tW + 2 + uW) / 2;
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(3);
  tft.drawString(tstr, startX, 36);
  tft.setTextSize(2);
  tft.drawString("C", startX + tW + 2, 36);

  // Consigne
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(MUTED);
  tft.drawString("consigne " + String(lastData.setpoint, 1) + "C", 72, 60);

  // 2b) Colonne densite (centree en hauteur), centre sur x=176
  tft.setTextSize(3);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(SG_VIOLET);
  tft.drawFloat(lastData.gravity, 3, 176, 28);

  tft.setTextSize(1);
  tft.setTextColor(MUTED);
  if (lastData.gravityStart > 1.0f) {
    int att = (int)round((lastData.gravityStart - lastData.gravity) /
                         (lastData.gravityStart - 1.0f) * 100.0f);
    tft.drawString("OG " + String(lastData.gravityStart, 3), 176, 46);
    tft.drawString(String(att) + "%  " + String(lastData.angle, 0) + "deg", 176, 57);
  } else {
    tft.drawString("OG --", 176, 46);
    tft.drawString(String(lastData.angle, 0) + "deg", 176, 57);
  }

  // 2c) IP + compteur clients AP en pied (bas droite du bloc central)
  tft.setTextSize(1);
  tft.setTextDatum(BR_DATUM);
  tft.setTextColor(SKY);
  tft.drawString(lastData.ip, 224, 73);

  // Compteur clients du point d'acces, a gauche de l'IP.
  // Position calculee : la longueur de l'IP varie selon le DHCP.
  int ipW = tft.textWidth(lastData.ip);
  tft.setTextColor((lastData.apClients > 0) ? VERT : MUTED);
  tft.drawString("AP:" + String(lastData.apClients), 224 - ipW - 6, 73);

  // 3) Barres d'etat (droite)
  tft.drawFastVLine(228, 0, 76, TRACK);

  // Batterie
  uint16_t batCol = (lastData.battery > 50) ? VERT : (lastData.battery >= 20) ? ORANGE : ROUGE;
  drawVerticalBar(232, lastData.battery / 100.0f, batCol, String(lastData.battery));

  // Signal iSpindel
  float ispForce = clampf((lastData.iSpindelRssi + 100) / 50.0f, 0.0f, 1.0f);
  uint16_t ispCol = lastData.iSpindelOnline
                      ? ((ispForce >= 0.6f) ? VERT : (ispForce >= 0.3f) ? ORANGE : ROUGE)
                      : ROUGE;
  drawVerticalBar(249, lastData.iSpindelOnline ? ispForce : 0.1f, ispCol, "iS");

  // Signal WiFi
  // RSSI a 0 = STA deconnectee (convention de main.cpp), pas un signal parfait
  float wifiForce;
  uint16_t wifiCol;
  if (lastData.wifiRssi == 0) {
    wifiForce = 0.1f;
    wifiCol   = ROUGE;
  } else {
    wifiForce = clampf((lastData.wifiRssi + 100) / 50.0f, 0.0f, 1.0f);
    wifiCol   = (wifiForce >= 0.6f) ? VERT : (wifiForce >= 0.3f) ? ORANGE : ROUGE;
  }
  drawVerticalBar(266, wifiForce, wifiCol, "Wi");
}

// Barre SORTIE verticale (REPOS / FROID / CHAUD), texte empile
void DisplayManager::drawOutputBar() {
  uint16_t bg = REPOS_BG, txt = REPOS_FG, brd = REPOS_BRD;
  String label = "REPOS";
  if (lastData.coolOn)      { bg = FROID; brd = FROID; txt = TFT_BLACK; label = "FROID"; }
  else if (lastData.heatOn) { bg = CHAUD; brd = CHAUD; txt = TFT_BLACK; label = "CHAUD"; }

  tft.fillRoundRect(3, 6, 14, 64, 2, bg);
  tft.drawRect(3, 6, 14, 64, brd);

  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(txt);
  int n = label.length();
  for (int i = 0; i < n; i++) {
    tft.drawString(String(label[i]), 10, 12 + i * 12);
  }
}

// Barre d'etat verticale pleine hauteur + label dessous
void DisplayManager::drawVerticalBar(int x, float fraction, uint16_t color, const String& label) {
  tft.drawRect(x, 6, 14, 56, TRACK);
  int h = (int)(clampf(fraction, 0.0f, 1.0f) * 54.0f);
  if (h > 0) tft.fillRect(x + 1, 6 + (54 - h) + 1, 12, h, color);
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(MUTED);
  tft.drawString(label, x + 7, 68);
}

float DisplayManager::clampf(float value, float lo, float hi) {
  if (value < lo) return lo;
  if (value > hi) return hi;
  return value;
}
