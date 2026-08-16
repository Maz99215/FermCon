# FermCon - MAJ affichage ecran (paysage 284x76)

## Fichiers modifies
- include/DisplayManager.h   : struct DisplayData + 2 champs (wifiRssi, iSpindelRssi), helpers prives revus.
- src/DisplayManager.cpp     : layout paysage complet (setRotation(1)).
- src/main.cpp               : 2 valeurs ajoutees a l'init DisplayData (WiFi.RSSI(), ispindel.getRSSI()).

## Nouveau layout (284x76)
- Gauche  : barre verticale SORTIE unique (REPOS / FROID / CHAUD).
- Centre  : temperature dominante + consigne | densite SG (grande, centree) + OG/att/angle | IP en pied (teinte marquee).
- Droite  : 3 barres verticales pleine hauteur = batterie, signal iSpindel, signal WiFi.
- Retires : nom de biere, barre d'avancement de profil, last-seen texte, MQTT texte.

## Points d'attention
- LGFX_Config.hpp INCHANGE. Les offsets GRAM (offset_x=82 / offset_y=18) sont cales pour rotation 0.
  En paysage (setRotation(1)) LovyanGFX recalcule les offsets, MAIS un decalage/miroir residuel est
  possible selon le module : si l'image est decalee, tester setRotation(3) puis ajuster offset_x/offset_y.
- Rien d'autre a installer (memes libs).

## Build
    pio run -e esp32-c6
    pio run -e esp32-c6 -t upload      # + upload data/ si besoin (LittleFS)
