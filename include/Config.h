#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Configuration des broches
#define PIN_DS18B20 4  // 1-Wire, pull-up 4.7k
#define PIN_COOL 2     // FROID : module relais opto-isole, canal vers compresseur frigo
#define PIN_HEAT 3     // CHAUD : module relais opto-isole, canal vers plaque 25W

// ---------------------------------------------------------------------------
// Niveaux actifs des broches
// ---------------------------------------------------------------------------
#define COOL_ACTIVE_LEVEL LOW  // voie froid active bas
#define HEAT_ACTIVE_LEVEL LOW  // voie chaud active bas

// Configuration de l'ecran TFT
#define PIN_TFT_SCLK 6
#define PIN_TFT_MOSI 7
#define PIN_TFT_CS 10
#define PIN_TFT_DC 11
#define PIN_TFT_RST 21
#define PIN_TFT_BL 22
#define TFT_BL_PWM_FREQ 5000
#define TFT_BL_PWM_RES 8

// ---------------------------------------------------------------------------
// Bornes dures des parametres de regulation (ADR-001)
// ---------------------------------------------------------------------------
#define SETPOINT_MIN 0.0f
#define SETPOINT_MAX 35.0f

#define HYSTERESIS_MIN 0.2f
#define HYSTERESIS_MAX 5.0f

#define TEMP_OFFSET_MIN -5.0f
#define TEMP_OFFSET_MAX 5.0f

#define MIN_COMPRESSOR_DELAY_MIN 180
#define MIN_COMPRESSOR_DELAY_MAX 3600

#define COOL_MIN_ON_S_MIN 60
#define COOL_MIN_ON_S_MAX 1800

#define HEAT_MIN_ON_S_MIN 30
#define HEAT_MIN_ON_S_MAX 1800

#define MAX_ON_TIMEOUT_S_MIN 600
#define MAX_ON_TIMEOUT_S_MAX 86400

#define TEMP_READ_INTERVAL_MS_MIN 1000
#define TEMP_READ_INTERVAL_MS_MAX 30000

#define TEMP_PLAUSIBLE_MIN_C_MIN -20.0f
#define TEMP_PLAUSIBLE_MIN_C_MAX 5.0f

#define TEMP_PLAUSIBLE_MAX_C_MIN 30.0f
#define TEMP_PLAUSIBLE_MAX_C_MAX 60.0f

#define TEMP_FAULT_TRIP_S_MIN 10
#define TEMP_FAULT_TRIP_S_MAX 300

#define TEMP_FAULT_CLEAR_S_MIN 30
#define TEMP_FAULT_CLEAR_S_MAX 1800

#define MQTT_PORT_MIN 1
#define MQTT_PORT_MAX 65535

#define STEP_DURATION_S_MIN 60
#define STEP_DURATION_S_MAX 2592000

#define STEP_TEMP_C_MIN 0.0f
#define STEP_TEMP_C_MAX 35.0f

// ---------------------------------------------------------------------------
// Parametres de controle — valeurs par defaut
// ---------------------------------------------------------------------------
#define TEMP_HYSTERESIS_C 1.0f
#define COMPRESSOR_MIN_OFF_S 300
#define COOL_MIN_ON_S 120
#define HEAT_MIN_ON_S 60
#define MAX_ON_TIMEOUT_S 7200

// Valeurs de temperature
#define DS18B20_ERR_LOW -127.0f
#define DS18B20_ERR_HIGH 85.0f
#define TEMP_READ_INTERVAL_MS 2000
#define DEFAULT_SETPOINT_C 18.0f

// ---------------------------------------------------------------------------
// Securite de mesure et de repli
// ---------------------------------------------------------------------------
#define TEMP_PLAUSIBLE_MIN_C -10.0f
#define TEMP_PLAUSIBLE_MAX_C 50.0f
#define TEMP_FAULT_TRIP_S 60
#define TEMP_FAULT_CLEAR_S 300
#define RELAY_KEEPALIVE_TIMEOUT_S 30
#define WDT_TIMEOUT_S 10

// Point d'acces Wi-Fi (SoftAP)
#define DEFAULT_AP_SSID "FermCon"
#define DEFAULT_AP_PASSWORD "fermcon-setup"
#define AP_MIN_PASSWORD_LEN 8
#define AP_MAX_CLIENTS 4
#define WIFI_STA_CONNECT_TIMEOUT_MS 15000
#define WIFI_STA_RETRY_INTERVAL_MS 30000

// ---------------------------------------------------------------------------
// Synchronisation NTP
// ---------------------------------------------------------------------------
#define NTP_SERVER_1 "pool.ntp.org"
#define NTP_SERVER_2 "time.google.com"
#define NTP_TZ "CET-1CEST,M3.5.0,M10.5.0/3"
#define NTP_VALID_EPOCH_MIN 1600000000UL

// ---------------------------------------------------------------------------
// Version et constantes de l'application
// ---------------------------------------------------------------------------
#define CONFIG_SCHEMA_VERSION 2
#define PROFILE_SCHEMA_VERSION 2    // NOUVEAU v0.4.0 — schema du lot (/profile.json)
#define STEP_LABEL_MAX_LEN 24       // NOUVEAU v0.4.0 — label d'etape, 23 car. utiles + \0
#define FW_VERSION "0.4.0"
// v0.4.1 — iSpindel envoie toutes les ~15 min (900 s). Le timeout de 1500 s
// (25 min, soit ~1.67× l'intervalle) absorbe un envoi manque occasionnel
// ou une latence reseau sans declencher de faux hors-ligne.
#define ISPINDEL_ONLINE_TIMEOUT_S 1500
#define NTP_LOG_INTERVAL_MS 1000UL

#endif
