# Changelog FermCon

## [0.2.0] - 2026-08-20

### Ajouté

- **Wi-Fi AP+STA simultané permanent.** Le contrôleur héberge un SoftAP auquel l'iSpindel
  se connecte directement, tout en restant connecté en station au réseau domestique.
  Nouveaux défines : `DEFAULT_AP_SSID`, `DEFAULT_AP_PASSWORD`, `AP_MIN_PASSWORD_LEN`,
  `AP_MAX_CLIENTS`, `WIFI_STA_CONNECT_TIMEOUT_MS`, `WIFI_STA_RETRY_INTERVAL_MS`.
- Machine à états Wi-Fi non bloquante pour la reconnexion STA.
- Synchronisation NTP via `configTzTime()` avec fuseau Europe/Paris et heure d'été
  automatique. Défines `NTP_SERVER_1`, `NTP_SERVER_2`, `NTP_TZ`, `NTP_VALID_EPOCH_MIN`.
- Rampes de température dans les profils de fermentation, en complément des paliers.
- Distinction explicite entre lot démarré et lot non démarré : `isStarted()`,
  `getStartEpoch()`, champs `started` et `startEpoch` sur `/api/fermentation`, affichage
  « J-- » grisé sur le TFT et « Lot non démarré » dans l'interface web.
- Route `POST /api/restart`, redémarrage différé exécuté dans `WebServerManager::loop()`.
- Champs `sta_connected`, `ip_sta`, `ip_ap`, `ap_clients` sur `/api/status`.
- Champs de diagnostic de la régulation sur `/api/status` : `fault_count`,
  `last_fault_epoch`, `last_rejected_reading`, `fault_pending`, `has_valid_reading`.
- Contrôle de plausibilité des mesures de température, bornes `TEMP_PLAUSIBLE_MIN_C` et
  `TEMP_PLAUSIBLE_MAX_C`.
- Sentinelle keep-alive sur les sorties relais : `RelayController::keepAlive()` et
  `checkKeepAlive()`, délai `RELAY_KEEPALIVE_TIMEOUT_S`. Coupe les sorties si la
  régulation cesse de s'exécuter alors que la boucle principale tourne encore.
- Watchdog de tâche sur la boucle principale, délai `WDT_TIMEOUT_S`.
- Bouton de suppression d'étape dans l'éditeur de profil.

### Corrigé

- **Polarité de la sortie FROID.** `COOL_ACTIVE_LEVEL` valait `HIGH` alors que les deux
  canaux du module relais sont actifs à l'état bas. Le canal froid fonctionnait en logique
  inversée : relais énergisé au repos, désénergisé sur demande de froid. Conséquence
  induite : les deux canaux pouvaient être actifs simultanément sur une demande de chaud.
- **Claquement des relais au démarrage.** `RelayController::begin()` appelait
  `pinMode(OUTPUT)` avant d'écrire le niveau inactif, ce qui forçait la broche à 0 V et
  énergisait brièvement le relais. L'ordre est désormais : niveau inactif, `pinMode`,
  niveau inactif. `relays.begin()` est de plus la première instruction de `setup()`.
- **Exclusivité des sorties non garantie.** Toute commande passe désormais par
  `applyOutputs()`, qui coupe les deux sorties avant d'en activer une et refuse toute
  demande simultanée.
- **Extinction asymétrique dans `controlTemperature()`.** Chaque branche de demande ne
  gérait que sa propre sortie ; l'extinction n'existait que dans la branche « bande
  d'hystérésis ». En demande de froid, la sortie chaud restait donc active indéfiniment,
  et comme le chauffage empêchait le retour dans la bande d'hystérésis, la branche
  d'extinction n'était jamais atteinte. La sortie opposée est maintenant coupée dans les
  deux sens : chaud immédiatement, froid après `COOL_MIN_ON_S` pour protéger le
  compresseur.
- **Régulation active avant la première mesure.** `_currentTemp` valant `0.0` au
  démarrage, une demande de chaud était émise immédiatement. La régulation est désormais
  inhibée jusqu'à la première lecture plausible.
- **Défaut de sonde non temporisé.** Le défaut était déclaré à la première lecture
  invalide et levé silencieusement. Il est désormais déclaré après `TEMP_FAULT_TRIP_S` de
  lectures invalides consécutives et levé automatiquement après `TEMP_FAULT_CLEAR_S` de
  lectures plausibles consécutives, avec journalisation.
- **Collision de routes `/api/profile` et `/api/profile/activate`.**
  `AsyncCallbackWebHandler::canHandle()` accepte une URL commençant par l'URI enregistrée
  suivie d'un `/`. La route parente captait donc la sous-route et écrasait le profil en
  flash avec `stepCount = 0` à chaque activation. Les sous-routes sont désormais
  enregistrées avant leurs routes parentes.
- **Dérive du profil après redémarrage.** `ProfileManager` persistait un `millis()` ;
  au redémarrage, le calcul du temps écoulé débordait et la consigne sautait à la
  température de la dernière étape du profil. Risque thermique. La persistance se fait
  désormais sur un epoch UNIX, avec repli `DEFAULT_SETPOINT_C` et étape
  « Attente heure » si aucune référence de temps n'est disponible.
- Corps de requête POST partagé via un `static String` : remplacé par un tampon par
  requête sur `request->_tempObject`.
- Type d'étape de profil : le firmware n'acceptait qu'un entier alors que l'interface web
  envoyait du texte, rendant les rampes inutilisables. Les deux formes sont acceptées.
- Enregistrer un profil depuis l'interface web ne le désactive plus.
- Divers défauts de l'éditeur de profil web : lignes d'étapes disparaissant, champ de nom
  d'étape inopérant, activation effaçant la saisie en cours.
- Barre de signal Wi-Fi du TFT et compteur de clients AP.

### Supprimé

- **Route `POST /api/manual`** et son gestionnaire. Le contrôle manuel forçait une sortie
  sans passer par la logique de régulation, contournant l'exclusivité froid/chaud,
  l'anti-court-cycle du compresseur et la gestion de défaut de sonde.
- Dépendances `tzapu/WiFiManager`, incompatible avec le mode AP+STA permanent.

### Modifié

- `RelayController::begin()` n'antidate plus les horodatages d'extinction :
  l'anti-court-cycle est armé au démarrage. Après une mise sous tension, une demande de
  froid peut attendre jusqu'à `COMPRESSOR_MIN_OFF_S` avant d'énergiser le compresseur.

### Connu / non couvert

- Réception d'une trame iSpindel réelle sur `/ispindel` : jamais testée.
- Déclenchement effectif de la sentinelle keep-alive : non testé, aucun scénario de panne
  n'a été provoqué.
- Les paramètres de régulation restent figés à la compilation.
- `DisplayData` est toujours initialisé par agrégat positionnel.
- `requestTemperatures()` bloque la boucle principale environ 750 ms toutes les 2 s.

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
