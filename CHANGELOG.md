# Changelog FermCon

## v0.1.0 - 2026-07-07
Premier build **compilant et linkant** pour la cible ESP32-C6 (`pio run -e esp32-c6` = SUCCESS).
Flash 42,5 % (1,42 Mo / 3,34 Mo), RAM 15,6 %.

### Corrections de build / compatibilite ESP32-C6
- **OneWire** : remplacement de `paulstoffregen/OneWire` (incompatible ESP32-C6, acces
  registres GPIO typees) par `pstolarz/OneWireNg`, drop-in fournissant un `OneWire.h`
  compatible C6. `lib_ignore = OneWire` cible uniquement le OneWire de PaulStoffregen.
- **ElegantOTA** : ajout du flag `-D ELEGANTOTA_USE_ASYNC_WEBSERVER=1` -> mode AsyncWebServer
  (corrige l'ambiguite `HTTP_GET`/`HTTP_POST` et valide `begin(AsyncWebServer*)`).
- **Partitions** : `board_build.partitions = default_8MB.csv` + `flash_size = 8MB`
  (double slot OTA ~3,19 Mo/app + 1,5 Mo FS) -> le firmware 1,42 Mo depasse la table 4 Mo par defaut.

### Modules integres / corriges
- `ConfigStore` (SystemConfig/SystemStatus complets, persistance /config.json /profile.json /fermentation.json)
- `WebServerManager` (constructeur 7 args, routes profil + fermentation, handleISpindel multi-chunk, auth char[])
- `ProfileManager`, `DisplayManager`, `main.cpp` (integration complete, ElegantOTA async)
- Suppression de `src/WebServerManager_routes.cpp` (fusionne dans `WebServerManager.cpp`).

### Note de compatibilite Windows
Le chemin projet ne doit PAS contenir d'accents/espaces (ex. `Telechargements`) : GNU `ld`
echoue a creer `firmware.map`. Projet a placer sous un chemin ASCII (ex. `C:\dev\FermCon`).

### Reste a faire (post-v0.1)
- Flash + validation sur cible reelle (DS18B20/pull-up, polarites SSR froid / relais chaud).
- Calibration offsets ecran ST7789 (82/18).
