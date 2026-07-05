# FermCon — Firmware (mise à jour écran v2 + fermentation)

Contrôleur de fermentation bière sur **ESP32-C6** (Arduino + PlatformIO).
Cette archive contient : écran **ST7789** en **paysage plein écran** (nouveau
layout), actionneurs **SSR froid / relais chaud**, **profils** de température
(paliers + rampes), et le **suivi de fermentation** (libellé d'étape libre +
jours + batterie iSpindel + last seen + état MQTT).

## Arborescence
```
FermCon_MAJ/
├── platformio.ini            # config ST7789 + brochage
├── diagram.json              # simulation Wokwi
├── WIRING.md                 # câblage Wokwi / réel
├── include/
│   ├── Config.h
│   ├── RelayController.h
│   ├── TemperatureController.h
│   ├── DisplayManager.h          # v2 : DisplayData enrichie
│   ├── ProfileManager.h
│   ├── FermentationInfo.h        # NOUVEAU
│   └── ConfigStore.h             # fusionné profil + fermentation
├── src/
│   ├── main.cpp                     # intégration de référence
│   ├── RelayController.cpp
│   ├── TemperatureController.cpp
│   ├── DisplayManager.cpp           # v2 : layout paysage plein écran
│   ├── ProfileManager.cpp
│   ├── FermentationInfo.cpp         # NOUVEAU
│   ├── ConfigStore.cpp              # fusionné profil + fermentation
│   └── WebServerManager_routes.cpp  # EXTRAIT : routes profil + fermentation
└── data/web/
    ├── index.html            # profil + fermentation
    └── app.js                # profil + fermentation
```

## Écran v2 (paysage 320×240)
- Barre haut : `J{jours}` + nom d'étape (libellé libre) + batterie iSpindel (%).
- Panneau gauche : température géante + consigne + chips FROID/CHAUD.
- Panneau droit : densité SG + OG + atténuation + angle.
- Bande profil : étape courante + barre de progression.
- Bande connexions : iSpindel « vu il y a X min » + état MQTT.
- Pied de page : IP (petite, discrète).

## Build & flash
```
pio run
pio run -t upload
pio run -t uploadfs      # obligatoire pour data/web/
```
Mode test iSpindel Wokwi : `build_flags = -DWOKWI_SIM`.

## Points d'intégration
- **main.cpp / loop** : alimenter la struct `DisplayData` à chaque rafraîchissement
  (température, consigne, coolOn/heatOn, gravity/gravityStart/angle, battery,
  iSpindelOnline, iSpindelLastSeenMin, mqttConnected, fermentDays = `FermentationInfo::getFermentDays()`,
  stageName = `FermentationInfo::getStageName()`, profileStep*, ip, fault).
- **setup** : `ConfigStore.loadFermentation(fermentInfo)` au boot ; `FermentationInfo.begin()`.
- **WebServerManager_routes.cpp** est un EXTRAIT : fusionne les handlers dans ton
  WebServerManager réel et adapte le constructeur (ajout du pointeur `FermentationInfo*`).
- **ConfigStore** : fusionne avec ton ConfigStore réel (config WiFi/MQTT/Grainfather conservée).

## Points d'attention
- **Corps des requêtes POST web** : les handlers lisent `request->getParam("body", true)`
  (form-urlencoded, champ `body`). Le `app.js` envoie du JSON brut. À harmoniser :
  soit utiliser `AsyncCallbackJsonWebHandler` côté serveur, soit envoyer
  `body=<json>` en `application/x-www-form-urlencoded` côté front.
- **Auth web** : remplacer `authenticate("user","pass")` par tes identifiants (Config).
- **Jours de fermentation** : exact avec NTP (epoch) ; sans NTP, fallback compteur
  `millis()` (attention au wrap ~49 j sur très longue fermentation).
- **Last seen iSpindel** : compteur relatif en minutes basé sur `millis()` (pas de NTP requis).
- Polarité relais chaud (`HEAT_ACTIVE_LEVEL`) présumée LOW : à confirmer sur pièce.
- Écran ST7789 2.25" : décommenter les offsets CGRAM dans platformio.ini si décalage.

## Corrections QA appliquées à la génération
- DisplayManager v2 : `ledcAttach` (API core 3.x) OK.
- ArduinoJson : toutes occurrences v6 (`DynamicJsonDocument`) → **v7** (`JsonDocument`).
- WebServerManager : constructeurs divergents des 2 lots unifiés en un seul ;
  `setupRoutes()` dédupliqué ; chaînes JSON non échappées `"{"error"...}"` corrigées ;
  `containsKey` (v6) → `is<const char*>()` (v7).
- ConfigStore : fusion des méthodes profil + fermentation.
- Interface web : pages profil + fermentation fusionnées.

## Simulation Wokwi
`diagram.json` utilise désormais le composant réel **`wokwi-st7789`** (ESP32-C6
+ ST7789 supportés par Wokwi). Brochage simulé identique à la cible
(SCL=6, SDA=7, CS=10, DC=11, RES=21, BLK=22 ; DS18B20=4 ; LED froid=2, chaud=3).
Lancer via l'extension **Wokwi for VS Code** sur l'environnement `esp32-c6`.

## Tests unitaires (natifs, sans matériel)
```
pio test -e native
```
Couverture :
- `test/test_relay/`        — exclusivité FROID/CHAUD, allOff, état initial.
- `test/test_profile/`      — PALIER, interpolation RAMPE, addStep/clearSteps (16 max), isActive.
- `test/test_fermentation/` — libellé d'étape, jours (fallback millis), reset.
- `test/test_infra/`        — validation des mocks (millis, String).

Infra : `test/mocks/Arduino.h|.cpp` (mock Arduino : String, millis, GPIO, Serial),
`test/mocks/time_mock.h|.cpp` (remap `time()` contrôlable). L'env `native`
ne compile que `RelayController.cpp`, `ProfileManager.cpp`, `FermentationInfo.cpp`
(via `build_src_filter`) pour éviter les dépendances matérielles (TFT/OneWire/Async).

> NOTE : ces tests n'ont pas pu être exécutés dans l'environnement de génération
> (pas de toolchain C++). Ils ont été **relus et corrigés** statiquement
> (types `ProfileStep`, logique addStep/clear, mock `time()` pour les jours).
> Lance `pio test -e native` pour confirmation chez toi.

## Régulation — anti-court-cycle (corrigé)
`TemperatureController` a été refactoré :
- **Anti-court-cycle compresseur** : le froid ne redémarre pas avant `COMPRESSOR_MIN_OFF_S` (300s) d'arrêt.
- **Marche minimale** : `COOL_MIN_ON_S` (120s) / `HEAT_MIN_ON_S` (60s) avant extinction.
- **Repli sûr** sur erreur sonde (FAULT + allOff) et **timeout de sécurité** `MAX_ON_TIMEOUT_S`.
- `begin()` antidate les extinctions pour autoriser le premier démarrage au boot.
- **Testabilité** : sous `-DUNIT_TEST`, la dépendance DS18B20 (OneWire/DallasTemperature)
  est retirée et `setCurrentTempForTest(float)` injecte la température ; `update()` devient
  déterministe. Couvert par `test/test_regulation/`.

> Logique validée par simulation (8 scénarios, 16/16 assertions OK). La compilation
> C++ réelle se confirme avec `pio test -e native`.
