# FermCon — Contrôleur de fermentation (ESP32-C6)

Contrôleur autonome de fermentation de bière basé sur **ESP32-C6** (framework Arduino /
PlatformIO). Il régule la température d'une cuve (froid + chaud), reçoit les mesures d'un
**iSpindel**, les redistribue vers **MQTT** ou **Grainfather**, expose une **interface web**
protégée avec **OTA**, et affiche l'état sur un écran **ST7789**.

> État : **v0.1.0** — premier build compilant et linkant pour la cible ESP32-C6
> (`pio run -e esp32-c6` = SUCCESS, Flash 42,5 %, RAM 15,6 %). Voir `CHANGELOG.md`.

---

## Sommaire

- [Fonctionnalités](#fonctionnalités)
- [Matériel](#matériel)
- [Architecture logicielle](#architecture-logicielle)
- [Modules](#modules)
- [Prise en main (build & flash)](#prise-en-main-build--flash)
- [Configuration (`include/Config.h`)](#configuration-includeconfigh)
- [Interface web & API HTTP](#interface-web--api-http)
- [Format des données iSpindel / redistribution](#format-des-données-ispindel--redistribution)
- [Dépendances (PlatformIO `lib_deps`)](#dépendances-platformio-lib_deps)
- [Structure du projet](#structure-du-projet)
- [Dépannage](#dépannage)

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

> ℹ️ **Section rédigée séparément (agent matériel).** Le firmware attend le brochage
> défini dans `include/Config.h` ; le détail du câblage vit dans `WIRING.md`.
> Le contenu entre les marqueurs ci-dessous est géré hors du périmètre « code ».

<!-- HARDWARE:BEGIN -->
_(Section matériel à compléter : nomenclature, brochage, câblage, alimentation,
photos/schémas. Le firmware s'appuie sur les broches et polarités déclarées dans
`include/Config.h` — toute modification de brochage doit y être répercutée.)_
<!-- HARDWARE:END -->

---

## Architecture logicielle

Boucle Arduino classique (`setup()` / `loop()`) orchestrée dans `src/main.cpp`. Chaque
responsabilité est isolée dans un module, instancié une fois puis piloté par `loop()` :

```
                         ┌───────────────────────────────────────────────┐
                         │                  main.cpp                      │
                         │   setup(): begin() de tous les modules         │
                         │   loop() : cadence non bloquante (millis)      │
                         └───────────────────────────────────────────────┘
        WiFi / HTTP POST         │                 │                 │
   iSpindel ─────────────► ISpindelReceiver        │                 │
                                 │ (parsePayload)   │                 │
                                 ▼                  ▼                 ▼
                          FermentationInfo    DataPublisher     WebServerManager
                          (jours, étape,      (MQTT ou           (UI + API + OTA,
                           densité départ)     Grainfather)       auth simple)
                                 │                                     │
   DS18B20 ──► TemperatureController ──► RelayController ──► FROID / CHAUD (SSR/relais)
                     ▲   (hystérésis, anti-court-cycle,
                     │    FAULT, timeout)
              ProfileManager (consigne = f(temps) via paliers + rampes)
                                 │
   ConfigStore (LittleFS: /config.json, /profile.json, /fermentation.json)
                                 │
                          DisplayManager (ST7789, LovyanGFX)
```

**Principes clés :**
- **La régulation est prioritaire et autonome** : elle tourne même sans réseau. Un échec
  WiFi/MQTT/redistribution ne bloque jamais `TemperatureController`.
- **Non bloquant** : aucune fonction longue dans `loop()` ; les périodes (lecture sonde,
  reconnexion WiFi, refresh écran) sont cadencées via `millis()`.
- **Persistance** sur LittleFS : configuration, profil actif et infos de fermentation
  survivent au redémarrage.

---

## Modules

| Module (`include/` + `src/`) | Rôle |
|---|---|
| `main` | Point d'entrée : instancie les modules, câble les dépendances, orchestre `loop()`. |
| `Config.h` | Constantes de compilation : brochage, polarités, seuils de régulation, timings. |
| `ConfigStore` | Persistance LittleFS. Expose `SystemConfig` (wifi, mqtt, grainfather, auth, consigne, hystérésis…) et `SystemStatus`. `save()/load()`, `saveProfile()/loadProfile()`, `saveFermentation()/loadFermentation()`. |
| `TemperatureController` | Machine à états IDLE / COOLING / HEATING / FAULT. Hystérésis, anti-court-cycle, timeout, repli sûr sonde. Lit le DS18B20 (DallasTemperature / OneWireNg). |
| `RelayController` | Pilotage bas niveau des 2 sorties FROID / CHAUD avec exclusivité garantie et polarités paramétrables. |
| `ProfileManager` | Profils de fermentation (paliers + rampes, max 16 étapes). Interpolation de la consigne selon le temps écoulé. `toJson()/fromJson()`. |
| `FermentationInfo` | Suivi de la fermentation : jour courant, libellé d'étape (libre), densité de départ, atténuation. |
| `ISpindelReceiver` | Parsing du JSON iSpindel (`parsePayload()`), gestion des payloads multi-chunk. |
| `DataPublisher` | Redistribution des mesures vers MQTT (PubSubClient) **ou** Grainfather (HTTP POST). Reconnexion auto, jamais bloquant. |
| `WebServerManager` | Serveur async (ESPAsyncWebServer) : sert l'UI (`data/web`), API REST, auth HTTP Basic, intègre ElegantOTA. |
| `DisplayManager` | Rendu écran ST7789 via LovyanGFX (`LGFX_Config.hpp`), layout portrait 76×284, rétroéclairage PWM. |

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

## Configuration (`include/Config.h`)

Toutes les constantes de compilation (brochage, polarités, seuils) sont centralisées ici.
Les réglages runtime (WiFi, MQTT, consigne, auth…) sont modifiables via l'interface web et
persistés dans `/config.json`.

| Paramètre | Défaut | Description |
|---|---|---|
| Hystérésis | 1.0 °C | Bande morte autour de la consigne. |
| OFF min compresseur | 300 s | Délai mini avant redémarrage du froid (anti-court-cycle). |
| ON min froid / chaud | 120 s / 60 s | Marche minimale avant extinction. |
| Timeout maintien | 7200 s | Coupure de sécurité si une sortie reste ON anormalement longtemps. |
| Consigne par défaut | 18.0 °C | Utilisée sans profil actif. |
| Intervalle lecture sonde | 2000 ms | Période d'échantillonnage du DS18B20. |
| Erreurs sonde | -127 / 85 °C | Valeurs DS18B20 traitées comme FAULT. |

### Calibration écran
Le panneau 76×284 est plus petit que la GRAM du ST7789 : offsets de départ `82 / 18`
dans `include/LGFX_Config.hpp`, à ajuster de ±2 jusqu'à ce qu'un cadre plein écran touche
les 4 bords.

---

## Interface web & API HTTP

Servie depuis LittleFS (`data/web/`), protégée par **auth HTTP Basic** (identifiants dans
la config, à changer au premier boot). Endpoints principaux :

| Méthode | Route | Rôle |
|---|---|---|
| `GET` | `/` | Interface web (fichiers statiques depuis `/web/` sur LittleFS). |
| `POST` | `/ispindel` | Réception des mesures iSpindel (JSON, multi-chunk géré). |
| `GET` | `/api/status` | État temps réel (température, consigne, sorties, connexions). |
| `GET` / `POST` | `/api/config` | Lecture / mise à jour de la configuration. |
| `POST` | `/api/setpoint` | Réglage direct de la consigne. |
| `POST` | `/api/manual` | Commande manuelle des sorties FROID / CHAUD. |
| `GET` / `POST` | `/api/profile` | Lecture / édition du profil de fermentation. |
| `POST` | `/api/profile/activate` | Activation du profil (démarre le calcul de consigne). |
| `GET` / `POST` | `/api/fermentation` | Lecture / mise à jour des infos de fermentation. |
| `GET` / `POST` | `/update` | OTA firmware (ElegantOTA, même auth que l'UI). |

> 🔒 L'auth est volontairement simple (HTTP Basic). Ne pas exposer l'appareil directement
> sur Internet ; le garder sur le réseau local.

---

## Format des données iSpindel / redistribution

Payload iSpindel attendu sur `POST /ispindel` :

```json
{ "name": "iSpindel", "ID": 1, "temperature": 21.5, "temp_units": "C",
  "gravity": 1.050, "angle": 45.0, "battery": 3.7, "RSSI": -60 }
```

Redistribution (configurable à chaud) :
- **MQTT** (Mosquitto) : publication des champs sur le broker configuré.
- **Grainfather** : HTTP POST au format iSpindel, avec le nom de device suffixé `_SG`.

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
├── platformio.ini      Environnement esp32-c6 (cible)
├── WIRING.md           Câblage matériel
├── CHANGELOG.md
└── VERSION
```

---

## Dépannage

| Symptôme | Cause probable / solution |
|---|---|
| `ld` échoue sur `firmware.map` | Chemin du projet avec accent/espace → déplacer dans `C:\dev\FermCon`. |
| Erreurs `OneWire` à la compilation | Vérifier `lib_ignore = OneWire` (neutralise PaulStoffregen au profit d'OneWireNg). |
| Température figée à -127 / 85 °C | Défaut de câblage DS18B20 / pull-up → l'état passe en FAULT, sorties coupées. |
| UI web absente après flash | Oublier `uploadfs` : refaire `pio run -e esp32-c6 -t buildfs && ... -t uploadfs`. |
| Le froid ne redémarre pas | Normal si < 5 min depuis la dernière extinction (anti-court-cycle). |
| Écran décentré / rogné | Ajuster les offsets `82 / 18` dans `include/LGFX_Config.hpp`. |
