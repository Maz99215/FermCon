# FermCon

Contrôleur de fermentation de bière sur **ESP32-C6**, avec régulation thermique,
suivi de densité par **iSpindel**, afficheur TFT et interface web embarquée.

Version : **0.2.0**
Cible : `esp32-c6-devkitc-1` — PlatformIO / pioarduino, framework Arduino (core 3.x, ESP-IDF 5.x)

---

## 1. Ce que fait FermCon

- Régule la température d'une cuve via deux sorties relais (froid / chaud), avec hystérésis,
  temporisation anti-court-cycle du compresseur et exclusivité structurelle des deux sorties.
- Reçoit les mesures de densité d'un **iSpindel** qui se connecte **directement au point
  d'accès Wi-Fi hébergé par le contrôleur**. Aucun routeur intermédiaire n'est nécessaire.
- Reste simultanément connecté au réseau domestique en mode station, pour l'accès à
  l'interface web et la synchronisation de l'heure.
- Applique un profil de fermentation multi-étapes : paliers et rampes de température.
- Affiche l'état courant sur un bandeau TFT ST7789 de 284 x 76 pixels.
- Expose une interface web et une API HTTP JSON, protégées par authentification.

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

## 5. Paramètres de `Config.h`

| Défine | Valeur | Rôle |
|---|---|---|
| `DEFAULT_SETPOINT_C` | 18,0 | consigne par défaut |
| `TEMP_HYSTERESIS_C` | 1,0 | demi-bande d'hystérésis |
| `COMPRESSOR_MIN_OFF_S` | 300 | arrêt minimal du compresseur avant redémarrage |
| `COOL_MIN_ON_S` | 120 | marche minimale du froid |
| `HEAT_MIN_ON_S` | 60 | marche minimale du chaud |
| `MAX_ON_TIMEOUT_S` | 7200 | coupure de sécurité sur maintien anormalement long |
| `TEMP_READ_INTERVAL_MS` | 2000 | période d'acquisition |
| `TEMP_PLAUSIBLE_MIN_C` | −10,0 | borne basse de plausibilité |
| `TEMP_PLAUSIBLE_MAX_C` | 50,0 | borne haute de plausibilité |
| `TEMP_FAULT_TRIP_S` | 60 | invalides consécutives avant déclaration de défaut |
| `TEMP_FAULT_CLEAR_S` | 300 | valides consécutives avant levée de défaut |
| `RELAY_KEEPALIVE_TIMEOUT_S` | 30 | délai de la sentinelle de régulation |
| `WDT_TIMEOUT_S` | 10 | délai du watchdog de tâche |
| `AP_MIN_PASSWORD_LEN` | 8 | longueur minimale du mot de passe AP |
| `AP_MAX_CLIENTS` | 4 | clients simultanés sur le SoftAP |
| `NTP_VALID_EPOCH_MIN` | 1600000000 | seuil de validité d'un timestamp |

Ces valeurs sont figées à la compilation. Les rendre modifiables depuis l'interface web
est prévu dans un chantier ultérieur.

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

- identifiants web réinitialisés (repli `admin` / `admin`),
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

# 3. Restaurer immédiatement (identifiants par défaut admin/admin)
$b64 = [Convert]::ToBase64String([Text.Encoding]::ASCII.GetBytes("admin:admin"))
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
`username`, `password`, plus les réglages de régulation.

- Un champ mot de passe **vide ou absent** signifie « ne pas changer ».
- Le mot de passe AP doit faire au moins 8 caractères (`AP_MIN_PASSWORD_LEN`).
- Réponse : `{"status":"success","reboot_required":true|false}`.
- `400` si la configuration AP est invalide.

### `POST /api/setpoint`

Corps : `{"setpoint":18.5}`. Modifie la consigne manuelle, utilisée uniquement quand
aucun profil n'est actif.

### `GET /api/profile`

```json
{"name":"Ale","steps":[{"type":1,"tempStart":18,"tempEnd":22,"durationS":7200}],
 "startEpoch":1786971610,"startTime":1786971610,"active":true,
 "currentStep":"Rampe 18.00->22.00C (0%)","setpoint":18.04}
```

- `type` : `0` = palier, `1` = rampe. La lecture accepte aussi le texte
  `"PALIER"` / `"RAMPE"` pour compatibilité.
- `durationS` en secondes. 16 étapes maximum.
- `startTime` est un alias de `startEpoch`, conservé pour compatibilité.
- `currentStep` peut valoir `"Termine"` ou `"Attente heure"`.

### `POST /api/profile`

Corps : `{"name":..., "steps":[...], "active":...}`.
Si `active` est absent, l'état courant est **conservé** : éditer un profil ne le
désactive pas.

### `POST /api/profile/activate`

Corps : `{"active":true|false}`. Réponse : `Profile activation updated`.

**À enregistrer avant `/api/profile`** dans `begin()` — voir section Pièges connus.

### `GET` et `POST /api/fermentation`

```json
{"stageName":"Primaire","fermentDays":3,"started":true,"startEpoch":1786971610}
```

`POST` accepte `{"stageName":"..."}`, `{"action":"start"}` ou `{"action":"reset"}`.

### `POST /api/restart`

Réponse `{"status":"restarting","delay_ms":1000}`, redémarrage différé exécuté
dans `WebServerManager::loop()` afin que la réponse HTTP parte avant le reset.

### `POST /ispindel`

Route d'ingestion des trames iSpindel. **Sans authentification**, par conception.

### `/update`

Interface OTA ElegantOTA.

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
- Identifiants vides = repli `admin` / `admin`. **Changez-les après chaque `uploadfs`.**
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
| Reprise correcte du profil après redémarrage | validé |
| Paliers et rampes de température | validé |
| Distinction lot démarré / non démarré | validé |
| Polarité correcte des deux sorties | validé sur LED |
| Exclusivité froid / chaud dans les deux sens de transition | validé sur LED |
| Absence de claquement au démarrage (5 cycles secteur) | validé |
| Défaut sonde : coupure, puis levée automatique après reconnexion | validé |
| Anti-court-cycle compresseur | validé |
| Watchdog de tâche : aucun reset spurious | validé |
| Sentinelle keep-alive : déclenchement effectif | **non testé** — pas de scénario de panne provoqué |
| **Réception réelle d'une trame iSpindel sur `/ispindel`** | **non testé** |

Empreinte du binaire : **Flash 41,7 %**, **RAM 15,0 %**.

---

## 12. Reste à faire

- **Test de bout en bout avec un iSpindel réel.** Seule fonction majeure jamais exercée.
- Chantier interface : rendre configurables depuis le web l'hystérésis, les
  temporisations, les bornes de plausibilité et les seuils de défaut.
- Conversion de l'initialisation de `DisplayData` en initialisation nominale.
- Rendre `requestTemperatures()` non bloquant (`setWaitForConversion(false)`) : il bloque
  actuellement la boucle principale ~750 ms toutes les 2 s.
- Audit de sécurité du serveur web.
- Refonte de l'interface web.
- Écran plus grand (3,5" ST7796S 480x320) et évaluation d'un passage à l'ESP32-S3 :
  reportés à une version ultérieure.
- Réordonnancement des étapes de profil (la suppression d'étape est livrée).

---

<!-- HARDWARE:BEGIN -->

## Matériel

*Section renseignée par l'agent matériel. Ne pas modifier manuellement le contenu situé
entre les marqueurs `HARDWARE:BEGIN` et `HARDWARE:END`.*

<!-- HARDWARE:END -->
