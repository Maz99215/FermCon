#include "DisplayManager.h"

// Palette (RGB -> lgfx::color565)
constexpr uint16_t FROID     = lgfx::color565(14, 165, 233);
constexpr uint16_t CHAUD     = lgfx::color565(249, 115, 22);
constexpr uint16_t REPOS_BG  = lgfx::color565(17, 24, 39);
constexpr uint16_t REPOS_FG  = lgfx::color565(100, 116, 139);
constexpr uint16_t REPOS_BRD = lgfx::color565(51, 65, 85);
constexpr uint16_t SG_VIOLET = lgfx::color565(167, 139, 250);
constexpr uint16_t SKY       = lgfx::color565(56, 189, 248);
constexpr uint16_t VERT      = lgfx::color565(34, 197, 94);
constexpr uint16_t ORANGE    = lgfx::color565(245, 158, 11);
constexpr uint16_t ROUGE     = lgfx::color565(239, 68, 68);
constexpr uint16_t MUTED     = lgfx::color565(148, 163, 184);
constexpr uint16_t TRACK     = lgfx::color565(31, 41, 55);

DisplayManager::DisplayManager() : lastUpdateTime(0) {}

void DisplayManager::begin() {
  bool ok = tft.init();
  Serial.printf("[DISP] init=%d  w=%d h=%d\n", (int)ok, tft.width(), tft.height());

  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);

  ledcAttach(PIN_TFT_BL, TFT_BL_PWM_FREQ, TFT_BL_PWM_RES);
  setBacklight(70);
}

void DisplayManager::setBacklight(uint8_t percent) {
  ledcWrite(PIN_TFT_BL, map(percent, 0, 100, 0, 255));
}

void DisplayManager::update(const DisplayData& data) {
  unsigned long now = millis();
  if (now - lastUpdateTime < 500) return;   // throttle unique 500 ms
  lastUpdateTime = now;
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
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);

  // Header : J-- (muted) si aucun lot, sinon Jxx (sky) + nom d'etape (muted)
  char jbuf[16];
  if (lastData.batchStarted) {
    snprintf(jbuf, sizeof(jbuf), "J%u", lastData.fermentDays);
    tft.setTextColor(SKY);
  } else {
    snprintf(jbuf, sizeof(jbuf), "J--");
    tft.setTextColor(MUTED);
  }
  tft.drawString(jbuf, 22, 2);

  // Nom d'etape tronque a 10 caracteres
  char stageShort[11];
  strlcpy(stageShort, lastData.stageName, sizeof(stageShort));
  tft.setTextColor(MUTED);
  tft.drawString(stageShort, 22 + tft.textWidth(jbuf) + 3, 2);

  // Temperature (dominante) + unite C
  // NaN -> "--"
  char tbuf[16];
  if (isnan(lastData.currentTemp)) {
    snprintf(tbuf, sizeof(tbuf), "--");
  } else {
    snprintf(tbuf, sizeof(tbuf), "%.1f", lastData.currentTemp);
  }

  tft.setTextSize(3);
  int tW = tft.textWidth(tbuf);
  tft.setTextSize(2);
  int uW = tft.textWidth("C");
  int startX = 72 - (tW + 2 + uW) / 2;
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(3);
  tft.drawString(tbuf, startX, 36);
  tft.setTextSize(2);
  tft.drawString("C", startX + tW + 2, 36);

  // Consigne
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(MUTED);
  char cbuf[32];
  snprintf(cbuf, sizeof(cbuf), "consigne %.1fC", lastData.setpoint);
  tft.drawString(cbuf, 72, 60);

  // 2b) Colonne densite (centree en hauteur)
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(SG_VIOLET);
  if (isnan(lastData.gravity)) {
    tft.drawString("--", 176, 28);
  } else {
    char gbuf[8];
    snprintf(gbuf, sizeof(gbuf), "%.3f", lastData.gravity);
    tft.drawString(gbuf, 176, 28);
  }

  tft.setTextSize(1);
  tft.setTextColor(MUTED);
  if (!isnan(lastData.gravityStart) && lastData.gravityStart > 1.0f
      && !isnan(lastData.gravity) && lastData.gravity > 0.0f) {
    int att = (int)round((lastData.gravityStart - lastData.gravity) /
                         (lastData.gravityStart - 1.0f) * 100.0f);
    char ogbuf[32];
    snprintf(ogbuf, sizeof(ogbuf), "OG %.3f", lastData.gravityStart);
    tft.drawString(ogbuf, 176, 46);
    char abuf[32];
    snprintf(abuf, sizeof(abuf), "%d%%  %.0fdeg", att, isnan(lastData.angle) ? 0.0f : lastData.angle);
    tft.drawString(abuf, 176, 57);
  } else {
    tft.drawString("OG --", 176, 46);
    char abuf[16];
    snprintf(abuf, sizeof(abuf), "%.0fdeg", isnan(lastData.angle) ? 0.0f : lastData.angle);
    tft.drawString(abuf, 176, 57);
  }

  // 2c) IP + compteur clients AP en pied
  tft.setTextSize(1);
  tft.setTextDatum(BR_DATUM);
  tft.setTextColor(SKY);
  tft.drawString(lastData.ip, 224, 73);

  int ipW = tft.textWidth(lastData.ip);
  tft.setTextColor((lastData.apClients > 0) ? VERT : MUTED);
  char apbuf[16];
  snprintf(apbuf, sizeof(apbuf), "AP:%u", lastData.apClients);
  tft.drawString(apbuf, 224 - ipW - 6, 73);

  // 3) Barres d'etat (droite)
  tft.drawFastVLine(228, 0, 76, TRACK);

  // Batterie — 255 = inconnu -> "--"
  float batFrac;
  uint16_t batCol;
  char batLabel[8];
  if (lastData.batteryPct == 255) {
    batFrac = 0.0f;
    batCol = MUTED;
    snprintf(batLabel, sizeof(batLabel), "--");
  } else {
    batFrac = lastData.batteryPct / 100.0f;
    batCol = (lastData.batteryPct > 50) ? VERT : (lastData.batteryPct >= 20) ? ORANGE : ROUGE;
    snprintf(batLabel, sizeof(batLabel), "%u", lastData.batteryPct);
  }
  drawVerticalBar(232, batFrac, batCol, batLabel);

  // Signal iSpindel
  float ispForce;
  uint16_t ispCol;
  if (lastData.ispindelRssi == 0) {
    ispForce = 0.1f;
    ispCol = lastData.ispindelOnline ? MUTED : ROUGE;
  } else {
    ispForce = clampf((lastData.ispindelRssi + 100) / 50.0f, 0.0f, 1.0f);
    ispCol = lastData.ispindelOnline
              ? ((ispForce >= 0.6f) ? VERT : (ispForce >= 0.3f) ? ORANGE : ROUGE)
              : ROUGE;
  }
  drawVerticalBar(249, lastData.ispindelOnline ? ispForce : 0.1f, ispCol, "iS");

  // Signal WiFi
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

  // 4) Indicateur MQTT (petit point en bas a droite des barres)
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(lastData.mqttConnected ? VERT : MUTED);
  tft.drawString(lastData.mqttConnected ? "MQ" : "mq", 273, 73);

  // 5) Compteur de defauts sonde (affiche si > 0)
  if (lastData.faultCount > 0) {
    tft.setTextColor(ORANGE);
    tft.setTextDatum(TL_DATUM);
    char fbuf[16];
    snprintf(fbuf, sizeof(fbuf), "!%u", lastData.faultCount);
    tft.drawString(fbuf, 2, 2);
  }
}

// Barre SORTIE verticale (REPOS / FROID / CHAUD), texte empile
void DisplayManager::drawOutputBar() {
  uint16_t bg = REPOS_BG, txt = REPOS_FG, brd = REPOS_BRD;
  const char* label = "REPOS";
  if (lastData.coolOn)      { bg = FROID; brd = FROID; txt = TFT_BLACK; label = "FROID"; }
  else if (lastData.heatOn) { bg = CHAUD; brd = CHAUD; txt = TFT_BLACK; label = "CHAUD"; }

  tft.fillRoundRect(3, 6, 14, 64, 2, bg);
  tft.drawRect(3, 6, 14, 64, brd);

  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(txt);
  int n = strlen(label);
  for (int i = 0; i < n; i++) {
    char ch[2];
    ch[0] = label[i];
    ch[1] = '\0';
    tft.drawString(ch, 10, 12 + i * 12);
  }
}

// Barre d'etat verticale pleine hauteur + label dessous
void DisplayManager::drawVerticalBar(int x, float fraction, uint16_t color, const char* label) {
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
