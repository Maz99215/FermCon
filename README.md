# FermCon — Contrôleur de fermentation (ESP32-C6)

Contrôleur autonome de fermentation de bière basé sur **ESP32-C6** (framework Arduino /
PlatformIO). Il régule la température d'une cuve (froid + chaud), reçoit les mesures d'un
**iSpindel**, les redistribue vers **MQTT** ou **Grainfather**, expose une **interface web**
protégée avec **OTA**, et affiche l'état sur un écran **ST7789**.

> État : **v0.1.0** — premier build compilant et linkant pour la cible ESP32-C6
> (`pio run -e esp32-c6` = SUCCESS, Flash 42,5 %, RAM 15,6 %). Voir `CHANGELOG.md`.

---

## Fonctionnalités

- **Régulation de température** par hystérésis (±1 °C) avec 2 sorties exclusives FROID / CHAUD.
  - Anti-court-cycle compresseur : OFF minimum 5 min (300 s), marche minimale froid 120 s / chaud 60 s.
  - Repli sûr : sonde en erreur → les 2 sorties sont coupées (état FAULT).
  - Timeout de sécurité sur maintien anormalement long (`MAX_ON_TIMEOUT_S`).
  - Régulation **prioritaire et autonome**, fonctionne même hors ligne.
- **Profils de fermentation** : paliers + rampes persistants (LittleFS), consigne recalculée
  en fonction du temps écoulé.
- **Réception iSpindel** : endpoint HTTP POST JSON (`name, ID, temperature, temp_units,
  gravity, angle, battery, RSSI`).
- **Redistribution** configurable à chaud : **MQTT** (Mosquitto) *ou* **Grainfather**
  (HTTP POST au format iSpindel, device suffixé `_SG`).
- **Interface web** : réglages, profils, état temps réel — auth simple. **OTA** via ElegantOTA (`/update`).
- **Écran ST7789** 2.25" portrait (76×284) : température géante, consigne, densité (SG),
  jour/étape de fermentation, état des connexions, IP.

---

## Matériel

| Élément | Détail |
|---|---|
| MCU | ESP32-C6-DevKitC-1 (8 Mo flash) |
| Sonde température | DS18B20 (1-Wire) + pull-up **4.7 kΩ** |
| Sortie FROID | SSR-40DA → frigo, **actif HIGH** |
| Sortie CHAUD | Relais mécanique → plaque 25 W, **actif LOW** (à confirmer sur la pièce) |
| Écran | ST7789 SPI 2.25", 3.3 V |

### Brochage (hors pins strapping GPIO8/9/15 et USB natif GPIO12/13)

| Fonction | GPIO |
|---|---|
| DS18B20 (DQ) | **4** (pull-up 4.7 kΩ vers 3V3) |
| FROID (SSR) | **2** (actif HIGH) |
| CHAUD (relais) | **3** (actif LOW) |
| TFT SCLK | 6 |
| TFT MOSI | 7 |
| TFT CS | 10 |
| TFT DC | 11 |
| TFT RST | 21 |
| TFT rétroéclairage (BL, PWM) | 22 |

Les niveaux actifs et les broches sont paramétrables dans `include/Config.h`.
Voir `WIRING.md` pour le détail du câblage.

---

## Prise en main (build & flash)

Prérequis : [PlatformIO](https://platformio.org/) (CLI ou extension VSCode).

> ⚠️ **Windows** : placer le projet dans un chemin **sans accent ni espace**
> (ex. `C:\dev\FermCon`). Un accent (ex. `Téléchargements`) fait échouer GNU `ld`
> à la création de `firmware.map`.

```bash
# Compiler
pio run -e esp32-c6

# Flasher le firmware
pio run -e esp32-c6 -t upload

# Uploader le système de fichiers (interface web data/web) — à refaire à chaque modif de data/web
pio run -e esp32-c6 -t buildfs
pio run -e esp32-c6 -t uploadfs

# Moniteur série (115200)
pio device monitor -e esp32-c6
```

Au premier boot sans Wi-Fi configuré, WiFiManager ouvre un point d'accès de configuration.
Une fois connecté : interface sur `http://<IP>/`, OTA sur `http://<IP>/update`.

---

## Tests unitaires (PC, sans matériel)

```bash
pio test -e native
```

L'environnement `native` compile uniquement la logique testable (régulation, profils,
fermentation, relais) avec des mocks Arduino/temps — pas de dépendance TFT/OneWire/Async.
La CI GitHub Actions (`.github/workflows/ci.yml`) lance ces tests à chaque push / PR.

---

## Dépendances (PlatformIO `lib_deps`)

LovyanGFX · ArduinoJson 7 · ElegantOTA · WiFiManager · ESPAsyncWebServer + AsyncTCP ·
PubSubClient · DallasTemperature · **OneWireNg**.

Notes de compatibilité ESP32-C6 (déjà intégrées dans `platformio.ini`) :
- **OneWireNg** remplace `paulstoffregen/OneWire` (incompatible C6). `lib_ignore = OneWire`
  neutralise le OneWire de PaulStoffregen tiré en transitif.
- **ElegantOTA** en mode async via `-D ELEGANTOTA_USE_ASYNC_WEBSERVER=1`.
- Table de partitions **`default_8MB.csv`** + flash 8 Mo (double slot OTA + FS LittleFS).

---

## Structure du projet

```
FermCon/
├── include/            Headers (Config, ConfigStore, DisplayManager, WebServerManager,
│                       ProfileManager, RelayController, TemperatureController,
│                       ISpindelReceiver, DataPublisher, FermentationInfo, LGFX_Config.hpp)
├── src/                Implémentations (.cpp) + main.cpp
├── data/web/           Interface web (index.html, app.js, style.css) → LittleFS
├── test/               Mocks + suites Unity (relay, profile, fermentation, infra, regulation)
├── diagram.json        Simulation Wokwi (ESP32-C6 + ST7789 + LEDs)
├── platformio.ini      Environnements esp32-c6 (cible) et native (tests)
├── CHANGELOG.md
└── VERSION
```

---

## Réglages clés (`include/Config.h`)

| Paramètre | Défaut |
|---|---|
| Hystérésis | 1.0 °C |
| OFF min compresseur | 300 s |
| ON min froid / chaud | 120 s / 60 s |
| Timeout maintien | 7200 s |
| Consigne par défaut | 18.0 °C |
| Intervalle lecture sonde | 2000 ms |

### Calibration écran
Le panneau 76×284 est plus petit que la GRAM du ST7789 : offsets de départ `82 / 18`
dans `include/LGFX_Config.hpp`, à ajuster de ±2 jusqu'à ce qu'un cadre plein écran touche
les 4 bords.
