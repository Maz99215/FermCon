#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Configuration des broches
#define PIN_DS18B20 4  // 1-Wire, pull-up 4.7k
#define PIN_COOL 2     // FROID : SSR-40DA vers frigo
#define PIN_HEAT 3     // CHAUD : relais mécanique vers plaque 25W

// Niveaux actifs des broches
#define COOL_ACTIVE_LEVEL HIGH  // SSR actif haut
#define HEAT_ACTIVE_LEVEL LOW   // relais actif bas, à confirmer sur pièce

// Configuration de l'écran TFT
#define PIN_TFT_SCLK 6
#define PIN_TFT_MOSI 7
#define PIN_TFT_CS 10
#define PIN_TFT_DC 11
#define PIN_TFT_RST 21
#define PIN_TFT_BL 22
#define TFT_BL_PWM_FREQ 5000
#define TFT_BL_PWM_RES 8

// Paramètres de contrôle
#define TEMP_HYSTERESIS_C 1.0f
#define COMPRESSOR_MIN_OFF_S 300
#define COOL_MIN_ON_S 120
#define HEAT_MIN_ON_S 60
#define MAX_ON_TIMEOUT_S 7200

// Valeurs de température
#define DS18B20_ERR_LOW -127.0f
#define DS18B20_ERR_HIGH 85.0f
#define TEMP_READ_INTERVAL_MS 2000
#define DEFAULT_SETPOINT_C 18.0f

// Point d'accès Wi-Fi (SoftAP)
#define DEFAULT_AP_SSID "FermCon"
#define DEFAULT_AP_PASSWORD "fermcon-setup"
#define AP_MIN_PASSWORD_LEN 8
#define AP_MAX_CLIENTS 4
#define WIFI_STA_CONNECT_TIMEOUT_MS 15000
#define WIFI_STA_RETRY_INTERVAL_MS 30000

#endif