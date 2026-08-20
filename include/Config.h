#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Configuration des broches
#define PIN_DS18B20 4  // 1-Wire, pull-up 4.7k
#define PIN_COOL 2     // FROID : module relais opto-isole, canal vers compresseur frigo
#define PIN_HEAT 3     // CHAUD : module relais opto-isole, canal vers plaque 25W

// ---------------------------------------------------------------------------
// Niveaux actifs des broches
//
// Les deux canaux du module relais opto-isole sont ACTIFS LOW :
// entree IN au niveau bas = relais enclenche. Fait verifie sur piece le
// 17/08/2026, ce n'est plus une hypothese.
// Des resistances de rappel materielles de 10 kOhm vers 3V3 garantissent
// l'etat OFF hors pilotage.
// Le cablage materiel est la reference, le logiciel s'y aligne.
// Ces deux defines sont le POINT DE CONFIGURATION UNIQUE des niveaux actifs :
// aucun autre fichier ne doit ecrire HIGH ou LOW en dur sur ces broches.
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

// Parametres de controle
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
#define TEMP_PLAUSIBLE_MIN_C -10.0f   // en dessous : mesure jugee non plausible
#define TEMP_PLAUSIBLE_MAX_C 50.0f    // au dessus : mesure jugee non plausible
#define TEMP_FAULT_TRIP_S 60          // duree continue de mesures invalides avant declaration du defaut
#define TEMP_FAULT_CLEAR_S 300        // duree continue de mesures plausibles avant reprise apres defaut
#define RELAY_KEEPALIVE_TIMEOUT_S 30  // sans rafraichissement de la boucle de regulation, les sorties sont coupees
#define WDT_TIMEOUT_S 10              // chien de garde de tache : reset si la boucle principale se bloque

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
#define NTP_TZ "CET-1CEST,M3.5.0,M10.5.0/3"   // Europe/Paris, heure d'ete automatique
#define NTP_VALID_EPOCH_MIN 1600000000UL       // seuil de validite d'un timestamp
#define NTP_LOG_INTERVAL_MS 1000UL

#endif
