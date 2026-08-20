# Changelog FermCon

## [0.4.0] - 2026-08-20

Chantier interface : configuration de la régulation depuis le web, fusion du profil de
fermentation en une entité unique « Lot », et corrections issues des premiers essais sur
matériel réel.

### Ajouté

- **12 paramètres de régulation configurables à runtime**, sans recompilation :
  consigne (0,0–35,0 °C), hystérésis (0,2–5,0 °C), offset de sonde (−5,0–5,0 °C), délai
  anti-court-cycle du compresseur (180–3600 s), marche minimale froid (60–1800 s), marche
  minimale chaud (30–1800 s), timeout de sécurité de maintien (600–86400 s), intervalle de
  lecture sonde (1000–30000 ms), bornes de plausibilité basse/haute (−20,0 à 5,0 °C /
  30,0 à 60,0 °C), seuil de déclenchement du défaut sonde (10–300 s), seuil de levée du
  défaut sonde (30–1800 s). Chaque paramètre est borné (`ConfigValidator`), avec
  contraintes croisées (ex. écart mini plausibilité, cohérence AP) et écrêtage tracé en
  console série en cas de dépassement.
- **Entité « Lot »** : fusion du profil de fermentation (paliers/rampes) et du suivi de
  fermentation (jours écoulés, étape en cours) en une seule entité, exposée par un unique
  onglet web « Lot ». Chaque étape peut porter un nom libre (0 à 23 caractères) ; un nom
  vide produit un libellé automatique (`Palier 18.0C`, `Rampe 18.0->21.0C (42%)`).
- **Sélecteur d'unité de durée** par étape (minutes / heures / jours) dans le formulaire
  Lot. La conversion en secondes est automatique côté navigateur ; l'API continue de
  transporter la durée en secondes.
- **Lot actif sans étape** : un lot peut être démarré sans aucune étape définie. Il
  compte les jours écoulés et laisse la consigne manuelle piloter la régulation.
- **Champ `drives_setpoint`** (`/api/profile`, `/api/profile/activate`) : indique si le
  lot pilote effectivement la consigne (`actif ET au moins une étape`). La consigne
  manuelle n'est verrouillée que dans ce cas précis.
- **Champ de changement du mot de passe d'accès à l'interface**, avec confirmation de
  saisie, dans l'onglet Réseau. Suit le même principe que les mots de passe Wi-Fi/AP/MQTT
  déjà en place : laisser vide pour ne pas changer, jamais réaffiché après enregistrement.
- **Interface web republiée** : tableau de bord temps réel (~36 indicateurs), formulaires
  Régulation / Lot / Réseau / Intégrations avec validation côté client, temps restant du
  lot affiché dans l'unité la plus lisible (jours+heures, heures+minutes, ou minutes
  seules selon la durée restante).
- **API REST étendue** : `/api/status` complet (régulation, système, réseau, iSpindel,
  lot), `/api/config` avec bornes (`bounds`) et indicateurs `*_password_set` (dont le mot
  de passe d'accès), format d'erreur JSON normalisé (`code`, `message`, `field`, `min`,
  `max`).
- **Grainfather non bloquant** : machine à états (`GF_IDLE` / `GF_SENDING` /
  `GF_RETRY_WAIT`), sans `delay()` dans la boucle principale.
- **Persistance renforcée** : écriture atomique de `/config.json` et `/profile.json`
  (fichier temporaire + renommage), `schema_version` dans `/profile.json` pour les
  migrations futures.
- **Migration automatique du lot au démarrage** : si un ancien fichier de fermentation
  existe, son état (jours écoulés, démarrage) est repris dans le nouveau format Lot
  lorsque celui-ci est inactif et sans étape ; sinon l'utilisateur est invité à redémarrer
  son lot manuellement depuis l'interface.

### Modifié

- `TemperatureController` lit désormais tous ses paramètres depuis la configuration
  persistée : plus aucune constante de régulation figée à la compilation (voir §5 du
  README, qui documente les valeurs par défaut et leurs bornes).
- `POST /api/setpoint` : renvoie `409 PROFILE_ACTIVE` uniquement si le lot est actif **et**
  possède au moins une étape (`drives_setpoint`). Un lot actif sans étape laisse la
  consigne manuelle éditable.
- Boutons de l'interface : « Démarrer le lot » / « Arrêter le lot ».
- `Cache-Control` : `max-age=600` sur les fichiers statiques, `no-store` sur `/api/*`.
- Route 404 : réponse JSON normalisée `{"error":{"code":"NOT_FOUND","message":"Route inconnue"}}`.

### Supprimé

- Le suivi de fermentation séparé (ancien onglet « Fermentation », ancien fichier
  `/fermentation.json`) : entièrement absorbé par l'entité Lot ci-dessus. Les anciennes
  ancres `#profile` et `#fermentation` redirigent vers `#batch`.

### Corrigé

- **Débordement d'affichage de la densité iSpindel sur le TFT** : la valeur (ex.
  "1.048") débordait de sa colonne. Corrigé par une taille de police réduite et un
  formatage `snprintf` dédié.
- **Fuite mémoire sur les endpoints `GET`** (`/api/status`, `/api/config`, `/api/profile`) :
  des réponses HTTP étaient allouées puis jamais envoyées ni libérées sur certains
  chemins, épuisant progressivement le tas et provoquant des échecs d'authentification
  aléatoires après plusieurs minutes d'usage.
- **Chemin des fichiers statiques** : correction de la racine servie par le serveur web,
  qui pointait au mauvais niveau du système de fichiers LittleFS.
- **Faux statut hors-ligne de l'iSpindel** : le délai de détection (10 min) était plus
  court que l'intervalle d'envoi réel du capteur (15 min), ce qui déclenchait un
  hors-ligne erroné entre deux mesures. Porté à 25 min.
- **Timeout et tentatives Grainfather** : timeout HTTP réduit à 5 s, une seule tentative
  de nouvel essai, abandon immédiat (sans retry) sur réponse `429` (limite de débit
  atteinte côté Grainfather).
- **Valeur de batterie absente affichée comme température** : le code `255` (batterie non
  câblée) n'est plus interprété comme une mesure valide.
- **Étape de durée nulle** : une étape à `durationS == 0` est désormais ignorée par le
  calcul de consigne au lieu de bloquer la progression du lot.

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
