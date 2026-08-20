# FermCon

Contrôleur de fermentation de bière sur **ESP32-C6**, avec régulation thermique,
suivi de densité par **iSpindel**, afficheur TFT et interface web embarquée.

Version : **0.4.0**
Cible : `esp32-c6-devkitc-1` — PlatformIO / pioarduino, framework Arduino (core 3.x, ESP-IDF 5.x)

---

## 1. Ce que fait FermCon

- Régule la température d'une cuve via deux sorties relais (froid / chaud), avec hystérésis,
  temporisation anti-court-cycle du compresseur et exclusivité structurelle des deux sorties.
- Reçoit les mesures de densité d'un **iSpindel** qui se connecte **directement au point
  d'accès Wi-Fi hébergé par le contrôleur**. Aucun routeur intermédiaire n'est nécessaire.
- Reste simultanément connecté au réseau domestique en mode station, pour l'accès à
  l'interface web et la synchronisation de l'heure.
- Applique un **Lot** de fermentation multi-étapes (paliers et rampes de température),
  avec nom d'étape libre et sélecteur d'unité de durée (minutes / heures / jours) côté
  interface. Un lot peut aussi être démarré sans étape, pour suivre uniquement les jours
  écoulés.
- Affiche l'état courant sur un bandeau TFT ST7789 de 284 x 76 pixels.
- Expose une interface web et une API HTTP JSON, protégées par authentification, avec
  changement du mot de passe d'accès depuis l'interface (onglet Réseau).
- Régulation entièrement paramétrable depuis le web (12 paramètres bornés), sans
  recompilation ni reflash.

---

## 2. Architecture Wi-Fi : AP + STA simultanés

C'est le cœur de la version 0.2.0, inspiré du projet iSpindHub.

```
        iSpindel  ──Wi-Fi──►  SoftAP "FermCon" (192.168.4.1)
                                      │
                                 ESP32-C6  (WIFI_AP_STA)
                                      │
   Navigateur / PC  ◄──Wi-Fi──►  STA sur la box (192.168.1.x)
```

Le contrôleur fonctionne en permanence en `WIFI_AP_STA` :

- **SoftAP** : réseau dédié à l'iSpindel. IP `192.168.4.1`.
- **STA** : connexion à la box domestique, pour l'interface web et le NTP.

### Contraintes radio à connaître

- L'ESP32 n'a **qu'une seule radio**. Le SoftAP est donc forcé sur le canal utilisé par
  la connexion STA. Si la box change de canal, l'AP change aussi et **l'iSpindel décroche**.
  **Figez le canal Wi-Fi de la box sur 1, 6 ou 11.**
- L'ESP32 **ne route pas** entre le SoftAP et le LAN. Un client de l'AP ne peut pas
  atteindre Internet ni le réseau domestique. Ce n'est pas une limitation du firmware.
- La machine à états Wi-Fi est **non bloquante** et n'utilise jamais `WiFi.reconnect()`,
  incompatible avec le mode AP+STA.

### Paramétrage de l'iSpindel

| Champ iSpindel | Valeur |
|---|---|
| Service Type | **HTTP** (ni HTTPS Post, ni TCP, ni MQTT) |
| Server Address | `192.168.4.1` |
| Server Port | `80` |
| Server URL / URI | `/ispindel` |
| SSID | valeur de `ap_ssid`, par défaut `FermCon` |
| Password | valeur de `ap_password` |

La route `/ispindel` est **la seule route non authentifiée** du serveur : l'iSpindel n'a
pas à connaître les identifiants web.

---

## 3. Brochage

Valeurs **confirmées au banc** les 17 et 20/08/2026, cohérentes avec `include/Config.h`.

| Fonction | GPIO | Remarque |
|---|---|---|
| Sonde DS18B20 (OneWire) | 4 | résistance de tirage 4,7 kΩ vers 3,3 V |
| Sortie FROID (compresseur) | 2 | `COOL_ACTIVE_LEVEL LOW` |
| Sortie CHAUD (plaque 25 W) | 3 | `HEAT_ACTIVE_LEVEL LOW` |
| TFT SCLK | 6 | |
| TFT MOSI | 7 | |
| TFT CS | 10 | |
| TFT DC | 11 | |
| TFT RST | 21 | |
| TFT rétroéclairage | 22 | PWM via `ledcAttach`, 5000 Hz / 8 bits |

### Module relais

Module 2 canaux **opto-isolé**, cavalier `VCC`–`JD_VCC` **retiré** : côté opto alimenté en
3,3 V, côté bobines en 5 V. Résistance de tirage matérielle de 10 kΩ entre chaque entrée
`IN` et 3,3 V, pour maintenir l'état inactif pendant la phase de reset du microcontrôleur.

**Les deux canaux sont ACTIFS À L'ÉTAT BAS.** Une entrée `IN` au niveau bas énergise le
relais. C'est vérifié physiquement, pas déduit d'une documentation. Les versions
antérieures de ce README annonçaient `COOL_ACTIVE_LEVEL HIGH` : **c'était faux** et le
canal froid fonctionnait en logique inversée.

Les broches ne sont jamais écrites directement : tout passe par `RelayController`, seul
propriétaire de la traduction niveau logique / niveau électrique.

### Pins à ne pas utiliser sur ESP32-C6

GPIO8, GPIO9, GPIO15 sont des broches de strapping. GPIO12 et GPIO13 portent l'USB natif.

---

## 4. Sécurité des sorties

Cette section décrit les mécanismes de protection introduits par la version 0.2.0. Ils sont
tous **validés sur matériel**, sur LED seules puis avec les charges.

### Ordre de démarrage

`relays.begin()` est la **toute première instruction de `setup()`**, exécutée avant
`Serial.begin()`. Cette position est critique : ne pas la déplacer.

`RelayController::begin()` procède broche par broche : écriture du niveau inactif,
**puis** `pinMode(OUTPUT)`, **puis** réécriture du niveau inactif. L'ordre inverse
(`pinMode` d'abord) force la broche à 0 V le temps d'un cycle et **énergise brièvement le
relais** sur un module actif à l'état bas. C'est la cause du claquement audible au
démarrage que corrige cette version.

### Exclusivité structurelle

Toute commande de sortie passe par `applyOutputs(cool, heat)`, privé. Cette fonction :

- coupe les deux sorties **avant** d'en activer une ;
- refuse et journalise toute demande simultanée froid + chaud ;
- n'écrit jamais un niveau électrique brut choisi par l'appelant.

L'exclusivité ne repose donc pas sur la discipline du code appelant. Elle est garantie par
construction, ce qui a effectivement rattrapé un défaut de la couche de régulation pendant
la mise au point.

### Coupure asymétrique de la sortie opposée

Quand la demande s'inverse, la sortie en cours est coupée selon la nature de sa charge :

| Transition | Comportement |
|---|---|
| Demande de FROID alors que CHAUD est actif | CHAUD coupé **immédiatement**. Charge résistive, aucune contrainte mécanique. |
| Demande de CHAUD alors que FROID est actif | FROID coupé après `COOL_MIN_ON_S` (120 s) depuis son activation. Protection du compresseur. |

CHAUD **ne peut pas** être activé tant que FROID est actif : sinon `applyOutputs()`
couperait FROID pour activer CHAUD et contournerait silencieusement `COOL_MIN_ON_S`.

### Plausibilité des mesures et défaut temporisé

- Toute lecture hors de `[TEMP_PLAUSIBLE_MIN_C, TEMP_PLAUSIBLE_MAX_C]` est **rejetée**, la
  dernière valeur plausible est conservée. Cela couvre les codes d'erreur DS18B20 (`-127`,
  `+85`) mais aussi les valeurs aberrantes intermédiaires.
- Le défaut est déclaré après `TEMP_FAULT_TRIP_S` (60 s) de lectures invalides
  **consécutives**, pas à la première. Un parasite isolé ne coupe plus la régulation.
- Le défaut est levé **automatiquement** après `TEMP_FAULT_CLEAR_S` (300 s) de lectures
  plausibles consécutives, avec journalisation. Aucune intervention manuelle.
- En défaut, les deux sorties sont coupées et le maintien est inconditionnel.

### Aucune régulation avant la première mesure valide

`_currentTemp` vaut `0.0` au démarrage. Sans garde, cette valeur déclenche une demande de
chaud immédiate. La régulation est donc bloquée jusqu'à la première lecture plausible
(`hasValidReading()`).

### Sentinelle keep-alive

`TemperatureController::update()` réarme une sentinelle à chaque cycle de régulation
réussi. `relays.checkKeepAlive()`, appelé dans `loop()`, coupe les deux sorties si la
sentinelle n'a pas été réarmée depuis `RELAY_KEEPALIVE_TIMEOUT_S` (30 s). Cela couvre le
cas où la boucle principale tourne encore mais où la régulation ne s'exécute plus.

### Watchdog de tâche

La tâche `loop` est surveillée par le Task WDT avec un délai de `WDT_TIMEOUT_S` (10 s),
mode panic actif. Trois points d'implémentation à connaître :

- arduino-esp32 3.x initialise déjà le Task WDT. `esp_task_wdt_init()` renvoie alors
  `ESP_ERR_INVALID_STATE` ; le code bascule sur `esp_task_wdt_reconfigure()`.
- L'ESP32-C6 est **monocœur** : `idle_core_mask` vaut `0x01`. Un masque désignant un cœur
  inexistant fait échouer l'initialisation.
- La souscription (`esp_task_wdt_add`) a lieu **en fin de `setup()`**, pour ne pas
  surveiller les initialisations lentes (LittleFS, TFT, démarrage du SoftAP).

Marge disponible : `DallasTemperature::requestTemperatures()` bloque la boucle environ
750 ms toutes les 2 s. Confortable face aux 10 s du watchdog, mais c'est le premier
suspect si un traitement lourd est ajouté à `loop()`.

### Anti-court-cycle armé au démarrage

`begin()` **n'antidate plus** les horodatages d'extinction. Conséquence assumée : après une
mise sous tension, une demande de froid peut attendre jusqu'à `COMPRESSOR_MIN_OFF_S`
(300 s) avant d'énergiser le compresseur. Ce n'est pas un dysfonctionnement.

### Contrôle manuel supprimé

La route `POST /api/manual` **n'existe plus**. Elle permettait de forcer une sortie sans
passer par la régulation, donc de contourner l'exclusivité, l'anti-court-cycle et la
gestion de défaut. Ne pas la réintroduire.

---

## 5. Paramètres de régulation et de `Config.h`

**Depuis la v0.4.0, les 12 paramètres de régulation ci-dessous sont configurables depuis
l'interface web (`GET`/`POST /api/config`), sans recompilation.** Chaque paramètre est
borné côté firmware (`ConfigValidator`) ; toute valeur hors bornes est refusée
(`400 VALIDATION_ERROR`) ou écrêtée selon le point d'entrée, avec trace série.

| Paramètre | Défaut | Bornes | Rôle |
|---|---|---|---|
| `setpoint` | 18,0 °C | 0,0 – 35,0 °C | consigne manuelle (hors lot pilotant la consigne) |
| `hysteresis` | 1,0 °C | 0,2 – 5,0 °C | demi-bande d'hystérésis |
| `temp_offset` | 0,0 °C | −5,0 – 5,0 °C | offset appliqué à la lecture de sonde |
| `min_compressor_delay` | 300 s | 180 – 3600 s | arrêt minimal du compresseur avant redémarrage |
| `cool_min_on_s` | 120 s | 60 – 1800 s | marche minimale du froid |
| `heat_min_on_s` | 60 s | 30 – 1800 s | marche minimale du chaud |
| `max_on_timeout_s` | 7200 s | 600 – 86400 s | coupure de sécurité sur maintien anormalement long |
| `temp_read_interval_ms` | 2000 ms | 1000 – 30000 ms | période d'acquisition de la sonde |
| `temp_plausible_min_c` | −10,0 °C | −20,0 – 5,0 °C | borne basse de plausibilité |
| `temp_plausible_max_c` | 50,0 °C | 30,0 – 60,0 °C | borne haute de plausibilité |
| `temp_fault_trip_s` | 60 s | 10 – 300 s | invalides consécutives avant déclaration de défaut |
| `temp_fault_clear_s` | 300 s | 30 – 1800 s | valides consécutives avant levée de défaut |

Contrainte croisée C1 : `temp_plausible_max_c >= temp_plausible_min_c + 10.0`.

### Constantes restant figées à la compilation (`Config.h`)

| Défine | Valeur | Rôle |
|---|---|---|
| `RELAY_KEEPALIVE_TIMEOUT_S` | 30 | délai de la sentinelle de régulation |
| `WDT_TIMEOUT_S` | 10 | délai du watchdog de tâche |
| `AP_MIN_PASSWORD_LEN` | 8 | longueur minimale du mot de passe AP |
| `AP_MAX_CLIENTS` | 4 | clients simultanés sur le SoftAP |
| `NTP_VALID_EPOCH_MIN` | 1600000000 | seuil de validité d'un timestamp |
| `ISPINDEL_ONLINE_TIMEOUT_S` | 1500 (25 min) | délai avant statut « hors-ligne » de l'iSpindel. Porté de 600 s à 1500 s en v0.4.0 : l'iSpindel n'envoie une trame que toutes les ~15 min, l'ancien seuil de 10 min déclenchait un hors-ligne erroné entre deux envois. |
| `PROFILE_SCHEMA_VERSION` | 2 | version du format de `/profile.json`, pour les migrations futures |
| `STEP_LABEL_MAX_LEN` | 24 | longueur max du nom d'étape (23 caractères utiles + terminateur) |

Ces constantes matérielles/sécurité restent volontairement figées : elles touchent à la
protection du compresseur, au watchdog ou à des seuils réseau qui n'ont pas vocation à
être modifiés en exploitation courante.

---

## 6. Compilation et flash

```bash
pio run                 # compilation
pio run -t upload       # flash du firmware
pio run -t uploadfs     # flash de la partition LittleFS (data/web/)
pio device monitor      # console série
```

Le port série est un **USB-JTAG natif** (`303A:1001`). `monitor_dtr = 1` est
**obligatoire** dans `platformio.ini`, sinon les logs sont silencieusement perdus.

### ⚠ `uploadfs` détruit la configuration

`pio run -t uploadfs` réécrit **toute** la partition LittleFS, donc efface `config.json`.
Le firmware repart alors sur les valeurs par défaut de `Config.h`, ce qui signifie :

- identifiants web réinitialisés (repli `admin` / mot de passe vide),
- mot de passe du point d'accès revenu à sa valeur par défaut — **l'iSpindel ne se
  connecte plus**,
- réglages de régulation, profil et libellé d'étape perdus.

**Procédure obligatoire :**

```powershell
# 1. Sauvegarder
Invoke-RestMethod -Uri http://<ip>/api/config -Headers $hdr | ConvertTo-Json | Set-Content backup-config.json
curl.exe -s -u <user>:<pass> http://<ip>/api/profile | Set-Content backup-profile.json

# 2. Flasher
pio run -t uploadfs

# 3. Restaurer immédiatement (identifiants par défaut admin / mot de passe vide)
$b64 = [Convert]::ToBase64String([Text.Encoding]::ASCII.GetBytes("admin:"))
$hdr = @{ Authorization = "Basic $b64" }
Invoke-RestMethod -Uri http://<ip>/api/config -Method Post -Headers $hdr -ContentType 'application/json' `
  -Body '{"ap_enabled":true,"ap_ssid":"FermCon","ap_password":"<motdepasse>","username":"<user>","password":"<pass>"}'

# 4. Redémarrer (nécessaire pour qu'ElegantOTA reprenne les identifiants)
Invoke-RestMethod -Uri http://<ip>/api/restart -Method Post -Headers $hdr
```

---

## 7. API HTTP

Toutes les routes sont protégées par authentification HTTP, **sauf `/ispindel`**.
Le serveur émet un défi `WWW-Authenticate: Digest`, mais un en-tête `Basic` valide
est également accepté.

### `GET /api/status`

```json
{"temperature":23.94,"setpoint":18,"relay_fridge":false,"relay_heater":true,
 "uptime":33,"wifi_rssi":-43,"heap_free_kb":269,"temp_sensor_ok":true,
 "sta_connected":true,"ip_sta":"192.168.1.155","ip_ap":"192.168.4.1","ap_clients":0,
 "fault_count":0,"last_fault_epoch":0,"last_rejected_reading":0,
 "fault_pending":false,"has_valid_reading":true}
```

Champs de diagnostic de la régulation :

| Champ | Signification |
|---|---|
| `fault_count` | nombre de défauts déclarés depuis le démarrage. **Non persistant.** |
| `last_fault_epoch` | horodatage du dernier défaut. Vaut 0 si le défaut a précédé la synchro NTP. |
| `last_rejected_reading` | dernière valeur rejetée par le contrôle de plausibilité |
| `fault_pending` | un comptage d'invalides est en cours, défaut pas encore déclaré |
| `has_valid_reading` | au moins une mesure plausible a été acquise ; à `false`, la régulation est inhibée |

`ip_address` est conservé pour compatibilité ascendante.

### `GET /api/config`

Renvoie la configuration sans aucun secret. `ap_password_set` indique seulement
qu'un mot de passe est défini, jamais sa valeur.

### `POST /api/config`

Accepte notamment `wifi_ssid`, `wifi_password`, `ap_enabled`, `ap_ssid`, `ap_password`,
`username`, `password` (mot de passe d'accès à l'interface web), `mqtt_password`, plus
les 12 réglages de régulation du §5.

- Un champ mot de passe **vide ou absent** signifie « ne pas changer », pour tous les
  mots de passe (Wi-Fi, AP, accès interface, MQTT).
- Le mot de passe AP doit faire au moins 8 caractères (`AP_MIN_PASSWORD_LEN`). Le mot de
  passe d'accès à l'interface n'a pas de contrainte de longueur minimale.
- `GET /api/config` renvoie un indicateur `password_set` (bool) pour le mot de passe
  d'accès, sur le même principe que `wifi_password_set` / `ap_password_set` /
  `mqtt_password_set` — jamais la valeur elle-même.
- Réponse : `{"status":"success","reboot_required":true|false}`.
- `400 VALIDATION_ERROR` (avec `field`, `min`, `max`) si un paramètre de régulation est
  hors bornes ; `400` si la configuration AP est invalide.

### `POST /api/setpoint`

Corps : `{"setpoint":18.5}`. Modifie la consigne manuelle. Renvoie `409 PROFILE_ACTIVE`
uniquement si le lot est actif **et** possède au moins une étape (`drives_setpoint`
vrai) — un lot actif sans étape laisse la consigne manuelle éditable.

### `GET /api/profile`

Le profil de fermentation et le suivi de lot ont fusionné en une seule entité : il n'y a
plus de route `/api/fermentation` séparée (voir Breaking changes ci-dessous).

```json
{"name":"Ale","schema_version":2,
 "steps":[{"type":"RAMPE","label":"Montee douce","tempStart":18,"tempEnd":22,"durationS":7200}],
 "startEpoch":1786971610,"active":true,
 "currentStep":"Rampe 18.00->22.00C (0%)","setpoint":18.04,
 "ferment_days":3,"drives_setpoint":true}
```

- `type` : accepte le texte `"PALIER"` / `"RAMPE"` (forme canonique) ou l'entier `0`/`1`
  pour compatibilité en lecture.
- `label` : nom libre de l'étape, 0 à 23 caractères (`STEP_LABEL_MAX_LEN`). Vide → libellé
  automatique (`Palier 18.0C`, `Rampe 18.0->21.0C (42%)`).
- `durationS` en secondes, quelle que soit l'unité choisie côté interface (minutes/heures/
  jours — conversion faite dans le navigateur). 16 étapes maximum.
- `ferment_days` : jours écoulés depuis `startEpoch`, `0` si inactif ou horloge non
  synchronisée.
- `drives_setpoint` : `true` si le lot pilote effectivement la consigne (`active` et au
  moins une étape).
- `schema_version` : version du format de persistance, `2` depuis la v0.4.0.

### `POST /api/profile`

Corps : `{"name":..., "steps":[{"type":..., "label":..., "tempStart":..., "tempEnd":..., "durationS":...}], "active":...}`.
Si `active` est absent, l'état courant est **conservé** : éditer un profil ne le
désactive pas. Le `label` de chaque étape est validé (longueur ≤ 23, aucun caractère de
contrôle).

### `POST /api/profile/activate`

Corps : `{"active":true|false}`. Un lot peut être activé **sans aucune étape** : il compte
alors les jours écoulés sans piloter la consigne. Réponse : inclut `startEpoch` et
`drives_setpoint`.

**À enregistrer avant `/api/profile`** dans `begin()` — voir section Pièges connus.

### `POST /api/restart`

Réponse `{"status":"restarting","delay_ms":1000}`, redémarrage différé exécuté
dans `WebServerManager::loop()` afin que la réponse HTTP parte avant le reset.

### `POST /ispindel`

Route d'ingestion des trames iSpindel. **Sans authentification**, par conception.

### `/update`

Interface OTA ElegantOTA.

### Migration et suppression de `/api/fermentation`

Le suivi de fermentation (jours écoulés, étape en cours) faisait initialement l'objet
d'une entité et d'une route séparées de l'édition du profil. Les deux ont fusionné en une
entité unique « Lot » dès cette version : il n'existe donc **pas** de route
`/api/fermentation` dans le firmware livré — toute intégration externe doit utiliser
`/api/profile`. Au premier démarrage, si un ancien fichier de suivi de fermentation est
détecté et que le lot est inactif et sans étape, son état est repris automatiquement dans
le nouveau format ; dans le cas contraire (lot avec étapes en cours), l'utilisateur est
invité à redémarrer son lot manuellement depuis l'interface.

---

## 8. Horodatage et NTP

- L'heure est synchronisée par `configTzTime()` appelé à chaque transition
  « STA connecté », avec le fuseau `CET-1CEST,M3.5.0,M10.5.0/3` (Europe/Paris,
  heure d'été automatique).
- Un timestamp est considéré valide au-delà de `NTP_VALID_EPOCH_MIN` (1600000000).
- `ProfileManager` et `FermentationInfo` persistent un **epoch UNIX**, jamais un
  `millis()`. Un redémarrage ne fait donc plus dériver le profil ni le compteur de jours.
- Si l'heure n'est pas disponible, un repli `millis()` **valable pour la session
  courante uniquement** est utilisé, et n'est jamais écrit en flash.
- Si aucune référence de temps exploitable n'existe, `getCurrentSetpoint()` renvoie
  `DEFAULT_SETPOINT_C` et l'étape courante affiche `"Attente heure"`. **C'est un repli
  de sécurité volontaire** : il évite qu'un redémarrage envoie la consigne à la
  température de la dernière étape du profil.

---

## 9. Pièges connus et bonnes pratiques

### Routage ESPAsyncWebServer

`AsyncCallbackWebHandler::canHandle()` accepte une URL qui **commence par** l'URI
enregistrée suivie d'un `/`. `/api/profile` capte donc `/api/profile/activate` s'il est
enregistré avant lui.

**Règle : enregistrer toute sous-route AVANT sa route parente, et tout `server.on()`
avant `serveStatic("/")`.**

### Corps de requête POST

Ne jamais utiliser un `static String body` partagé : deux requêtes concurrentes se
mélangent. Utiliser un tampon **par requête** via `request->_tempObject`, alloué par
`malloc` — le destructeur d'ESP32Async libère ce pointeur avec `free()`.
Caster en `(const char*)` avant `deserializeJson` pour éviter le mode zéro-copie
d'ArduinoJson, qui laisserait le document pointer sur de la mémoire libérée.

### Initialisation de `DisplayData`

`main.cpp` remplit `DisplayData` par un **initialiseur d'agrégat positionnel**. Tout
ajout de champ dans la structure impose de mettre à jour la liste à la bonne position.
**Dette technique** : à convertir en initialisation nominale.

### PowerShell 5.1

- `curl.exe -d "{\"k\":v}"` **détruit le corps JSON** : il arrive vide côté firmware
  (`Content-Length: 2`). Vérifiez toujours `Content-Length` dans une trace `curl -v`.
- Préférer `Invoke-RestMethod -Body '{"k":v}'` avec des guillemets simples.
- `-AllowUnencryptedAuthentication` n'existe qu'en PowerShell 7+.

---

## 10. Sécurité informatique

- Authentification HTTP sur toutes les routes sauf `/ispindel`.
- `config.password_hash` stocke le mot de passe **en clair** : le projet cible un réseau
  local de confiance. À ne pas exposer sur Internet.
- Nom d'utilisateur par défaut `admin`, mot de passe vide par défaut. **Définissez un
  mot de passe (onglet Réseau, champ « Accès à l'interface ») après chaque `uploadfs`.**
- `ElegantOTA.setAuth()` n'est appelé qu'au démarrage : après un changement
  d'identifiants, **redémarrez** pour que l'interface OTA en tienne compte.
- Un audit de sécurité du serveur web reste à faire : protection de l'endpoint OTA,
  refus des mots de passe vides, limitation du nombre de tentatives.

---

## 11. État de validation au 20/08/2026

| Fonction | État |
|---|---|
| AP+STA simultanés, association iSpindel | validé sur matériel |
| Interface web accessible via AP et via STA | validé |
| API `ap_*`, `/api/restart`, compteur de clients AP | validé |
| Synchronisation NTP et fuseau Europe/Paris | validé |
| Reprise correcte du lot après redémarrage | validé |
| Paliers et rampes de température, cycle froid/chaud réel | validé sur matériel |
| Distinction lot démarré / non démarré, lot actif sans étape | validé |
| Polarité correcte des deux sorties | validé sur LED |
| Exclusivité froid / chaud dans les deux sens de transition | validé sur LED |
| Absence de claquement au démarrage (5 cycles secteur) | validé |
| Défaut sonde : coupure, puis levée automatique après reconnexion | validé |
| Anti-court-cycle compresseur | validé |
| Watchdog de tâche : aucun reset spurious | validé |
| **Réception réelle d'une trame iSpindel sur `/ispindel`** | validé sur matériel |
| 12 paramètres de régulation configurables via `/api/config` | validé sur matériel |
| Onglet Lot unique (fusion profil/fermentation), sélecteur d'unité de durée | validé sur matériel |
| Remontée Grainfather après activation | validé sur matériel |
| Changement du mot de passe d'accès à l'interface | validé sur matériel |
| Statut iSpindel : plus de faux hors-ligne à 15 min d'intervalle d'envoi | validé sur matériel |
| Sentinelle keep-alive : déclenchement effectif | **non testé** — pas de scénario de panne provoqué |

---

## 12. Reste à faire

- Conversion de l'initialisation de `DisplayData` en initialisation nominale.
- Rendre `requestTemperatures()` non bloquant (`setWaitForConversion(false)`) : il bloque
  actuellement la boucle principale ~750 ms toutes les 2 s.
- Audit de sécurité du serveur web (protection de l'endpoint OTA, refus des mots de passe
  vides, limitation du nombre de tentatives).
- Réordonnancement des étapes du lot (la suppression d'étape est livrée, pas le
  glisser-déposer / réordonnancement).
- Écran plus grand (3,5" ST7796S 480x320) et évaluation d'un passage à l'ESP32-S3 :
  reportés à une version ultérieure.
- Provoquer un scénario de panne réel pour valider le déclenchement de la sentinelle
  keep-alive.

---

<!-- HARDWARE:BEGIN -->

## Matériel

*Section renseignée par l'agent matériel. Ne pas modifier manuellement le contenu situé
entre les marqueurs `HARDWARE:BEGIN` et `HARDWARE:END`.*

Version matérielle : **v1.1** (SSR supprimé, deux voies sur module relais mécanique) —
dernière révision 2026-07-09, brochage reconfirmé au banc les 17 et 20/08/2026.
Le brochage GPIO fait foi au §3 ; cette section décrit l'architecture, la puissance,
la connectique et la nomenclature.

### Synoptique

```mermaid
flowchart LR
    PWR["Alim AC-DC 5 V / 2 A"] --> ESP["ESP32-C6-DevKitC-1"]
    DS["DS18B20 (cuve)"] -->|1-Wire GPIO4| ESP
    ESP -->|SPI| TFT["TFT ST7789 2,25\""]
    ESP -->|GPIO2 actif LOW → IN2| RLY["Module 2 relais opto-isolé"]
    ESP -->|GPIO3 actif LOW → IN1| RLY
    RLY -->|COM/NO + snubber| FRIGO["Frigo 230 V (FROID)"]
    RLY -->|COM/NO + snubber| PLAQUE["Plaque 25 W 230 V (CHAUD)"]
    ISP["iSpindel"] -.Wi-Fi/HTTP.-> ESP
    ESP -.Wi-Fi.-> HA["Web / MQTT / Home Assistant"]
```

- **Contrôleur** : ESP32-C6-DevKitC-1, alimenté en 5 V, logique intégralement 3,3 V.
- **Mesure** : DS18B20 étanche en 1-Wire, mode 3 fils (pas de parasite power).
- **Affichage** : TFT ST7789 2,25" SPI, zone utile 284 x 76 px, rétroéclairage en PWM.
- **Actionneurs** : module 2 relais mécaniques opto-isolés — canal 2 = FROID, canal 1 = CHAUD.
- **Puissance déportée** : la totalité du 230 V est câblée hors carte logique
  (borniers, fusibles, snubbers).
- **Alimentation** : module AC-DC 5 V / 2 A vers le rail 5 V ; le 3,3 V est fourni par le DevKit.

### Attribution des actionneurs (définitive)

| Charge | Actionneur | Commande | Niveau actif | Protections |
|---|---|---|---|---|
| **FROID — frigo (compresseur)** | Relais mécanique canal 2 (`IN2`) | GPIO2 | actif LOW | Snubber RC aux contacts + fusible T6,3 A 🔶 + câble ≥ 1,5 mm² |
| **CHAUD — plaque 25 W** | Relais mécanique canal 1 (`IN1`) | GPIO3 | actif LOW | Snubber RC aux contacts + fusible T3,15 A 🔶 + câble ≥ 0,75 mm² |

Le SSR-40DA initialement prévu pour le froid a été **retiré du projet**. Les deux voies
étant des relais mécaniques, le mode de défaut est **« contact ouvert »** : contrôleur
planté ou hors tension = frigo et plaque coupés, donc état sûr.

### Câblage — côté commande (TBT)

**Cavalier `VCC`–`JD_VCC` du module relais retiré.** Le bloc d'alimentation 3 broches est
sérigraphié `GND · VCC · JD_VCC`, avec un shunt d'usine entre `VCC` et `JD_VCC` :

```
   [ GND ]  [ VCC ]  [ JD_VCC ]
              └──shunt──┘   ← à retirer
```

Retirer le capuchon et le conserver, puis **vérifier au multimètre** l'absence de
continuité `VCC` / `JD_VCC` avant câblage. Sans cette séparation, un GPIO à 3,3 V laisse
environ 1,7 V sur l'entrée de l'optocoupleur et le relais risque de ne pas retomber
franchement.

| Composant A | Pin A | Composant B | Pin B | Signal | Notes |
|---|---|---|---|---|---|
| Alim 5 V | +5 V | ESP32-C6 | 5V (VIN) | Alim | Condensateur réservoir 470–1000 µF à l'entrée |
| Alim 5 V | GND | ESP32-C6 | GND | Masse | Masse commune en étoile, obligatoire |
| Alim 5 V | +5 V | Module relais | JD_VCC | Alim bobines | Consommation > 100 mA les deux voies actives |
| ESP32-C6 | 3V3 | Module relais | VCC | Alim opto | Référence logique 3,3 V |
| ESP32-C6 | GND | Module relais | GND | Masse | — |
| ESP32-C6 | GPIO2 | Module relais | IN2 | Commande FROID | Actif LOW + pull-up 10 kΩ vers 3V3 |
| ESP32-C6 | GPIO3 | Module relais | IN1 | Commande CHAUD | Actif LOW + pull-up 10 kΩ vers 3V3 |
| ESP32-C6 | 3V3 / GND | DS18B20 | VDD / GND | Alim | Mode 3 fils |
| ESP32-C6 | GPIO4 | DS18B20 | DQ | 1-Wire | Pull-up 4,7 kΩ DQ → 3V3, obligatoire |
| ESP32-C6 | 3V3 / GND | TFT ST7789 | VCC / GND | Alim | Découplage 100 nF au plus près |
| ESP32-C6 | GPIO6/7/10/11/21/22 | TFT ST7789 | SCLK/MOSI/CS/DC/RST/BL | SPI + PWM | Écran 3,3 V |

Les pull-ups matérielles de 10 kΩ sur `IN1` et `IN2` maintiennent les relais ouverts
pendant le reset et la phase de boot, avant que `relays.begin()` ne prenne la main.
Elles sont indispensables : la séquence logicielle décrite au §4 ne couvre pas la
fenêtre de reset matériel.

### Câblage — côté puissance (230 V, déporté)

```
Phase ──[fusible T]──► COM ──(contact)──► NO ──► Phase charge (frigo / plaque)
Neutre ────────────────────────────────────────► Neutre charge (direct)
Terre (PE) ────────────────────────────────────► Terre charge (direct)

Snubber RC ⇒ aux bornes du contact (COM–NO) de CHAQUE canal
```

- On commute **uniquement la phase**, jamais le neutre, jamais la terre.
- Utiliser **COM + NO** : charge coupée au repos.
- **Snubber RC** aux bornes de chaque contact ; le canal frigo, inductif, est le plus
  critique pour la durée de vie des contacts.
- Snubbers spécifiés pour le secteur (condensateur X2, RC ≥ 250 V AC), non polarisés.
- Fusibles **temporisés** : ils protègent le matériel et couvrent le risque incendie ;
  la protection des personnes reste assurée par le différentiel du tableau.
- **PE raccordée à toute masse métallique**, y compris le boîtier s'il est conducteur.

⚠ Le câblage 230 V, le calibre réel des fusibles et la conformité de l'installation
doivent être vérifiés sur place, et validés par une personne qualifiée avant mise sous
tension. Aucune indication à distance ne remplace ce contrôle.

### BOM

| # | Référence / composant | Quantité | Rôle | Interface | Tension | Source suggérée | Notes |
|---|---|---|---|---|---|---|---|
| 1 | ESP32-C6-DevKitC-1 | 1 | Contrôleur, Wi-Fi, régulation, web | USB / GPIO | 5 V in / 3,3 V logique | fourni | Éviter GPIO8/9/15 et GPIO12/13 |
| 2 | DS18B20 étanche | 1 | Sonde de température cuve | 1-Wire | 3,3 V | fourni | Mode 3 fils |
| 3 | Résistance 4,7 kΩ | 1 | Pull-up 1-Wire | — | — | générique | Entre DQ et 3V3 |
| 4 | Résistance 10 kΩ | 2 | Pull-up `IN1`/`IN2`, relais OFF au boot | — | — | générique | Ajout par rapport au module d'origine |
| 5 | TFT ST7789 2,25" | 1 | Affichage local | SPI | 3,3 V | fourni | Zone utile 284 x 76 px |
| 6 | Module 2 relais opto-isolé | 1 | FROID (`IN2`) + CHAUD (`IN1`) | GPIO x2 | bobine 5 V / commande 3,3 V | fourni | Cavalier `JD_VCC` retiré, actif LOW confirmé au banc |
| 7 | Snubber RC secteur | 2 | Protection des contacts | — | 230 V | générique | Aux bornes COM–NO de chaque canal |
| 8 | Alim AC-DC 5 V / 2 A | 1 | Alimentation TBT | — | 230 V → 5 V | générique | 🔶 Référence exacte à confirmer |
| 9 | Condensateur 470–1000 µF / 16 V | 1 | Réservoir sur le rail 5 V | — | 5 V | générique | Pics d'émission Wi-Fi |
| 10 | Condensateur 100 nF | 1–2 | Découplage HF | — | — | générique | Près de l'ESP32 et de l'écran |
| 11 | Fusible T6,3 A + porte-fusible | 1 | Protection voie froid | — | 230 V | générique | 🔶 Calibre à confirmer selon le frigo réel |
| 12 | Fusible T3,15 A + porte-fusible | 1 | Protection voie chaud | — | 230 V | générique | 🔶 À confirmer selon la plaque |
| 13 | Borniers à vis, presse-étoupes, câblage, boîtier | lot | Connectique et mécanique | — | 230 V / TBT | générique | 🔶 Séparation physique TBT / secteur |
| 14 | Plaque chauffante ~25 W | 1 | Élément chauffant de l'enceinte | 230 V | 230 V | fournie | 🔶 Puissance réelle à confirmer |
| 15 | Frigo / réfrigérateur | 1 | Élément refroidissant | 230 V | 230 V | existant | Charge inductive |
| 16 | iSpindel | 1 | Suivi de densité | Wi-Fi / HTTP | — | existant | Client du SoftAP, voir §2 |
| — | ~~SSR-40DA~~ | ~~1~~ | ~~Commande froid~~ | — | — | — | ❌ Retiré du projet |

Statut : les éléments 1 à 7, 9, 10, 14 à 16 sont en place et validés au banc. Les
éléments marqués 🔶 restent à confirmer ou à finaliser sur l'installation définitive.

### Sécurité matérielle — points clés

- **Défaut « ouvert » sur les deux voies** : perte d'alimentation ou plantage du
  contrôleur = frigo et plaque coupés.
- **Aucun 230 V sur la carte logique** ; puissance en zone dédiée, séparée physiquement.
- **Isolation galvanique effective** grâce au cavalier `JD_VCC` retiré.
- **Pull-ups 10 kΩ** garantissant l'état OFF pendant le reset et le boot.
- **Snubbers RC** sur chaque contact, priorité au canal frigo.
- **Anti-court-cycle compresseur** en logiciel (voir §4 et §5).
- **Repli sûr sonde** : lectures invalides persistantes → les deux sorties sont coupées.
- Niveaux logiques homogènes en 3,3 V : **aucun conflit 3,3 V / 5 V** côté signaux. Le 5 V
  n'alimente que les bobines, derrière les optocoupleurs.

### Points matériels à surveiller

- **Canal Wi-Fi** : radio unique, SoftAP asservi au canal STA — figer le canal de la box
  sur 1, 6 ou 11, sinon l'iSpindel décroche (voir §2).
- **Pics de courant Wi-Fi** : l'alimentation 5 V et le condensateur réservoir sont
  dimensionnés pour cela ; une alimentation sous-dimensionnée provoque des resets.
- **Thermique** : le boîtier doit rester ventilé, l'alimentation AC-DC et le module relais
  ne doivent pas être confinés contre l'électronique logique.
- **Longueur du câble DS18B20** : au-delà de quelques mètres, la fiabilité 1-Wire se
  dégrade 🔶 — surveiller le compteur de lectures rejetées (`last_rejected_reading`).
- **Placement de la sonde** : la mesure doit refléter la température du moût, pas celle de
  l'air de l'enceinte, sinon l'hystérésis oscille.

<!-- HARDWARE:END -->
