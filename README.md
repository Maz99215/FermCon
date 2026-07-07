# FermCon - Migration TFT_eSPI -> LovyanGFX (ESP32-C6 + ST7789 76x284)

## Pourquoi
TFT_eSPI ne compile pas sur ESP32-C6 (Arduino Core 3.x / IDF 5.x) :
acces registres GPIO et SPI incompatibles. Lib de facto abandonnee.
LovyanGFX supporte nativement le C6 et couvre ~90% de l'API TFT_eSPI.

## Contenu
- `include/LGFX_Config.hpp` : config ecran (equivalent de ton User_Setup / build_flags).

## 1. platformio.ini
Dans `[env:esp32-c6]` :

a) Remplacer dans `lib_deps` :
   -  bodmer/TFT_eSPI
   +  lovyan03/LovyanGFX@^1.2.7

b) SUPPRIMER tous les build_flags TFT_eSPI (ils ne servent plus) :
   -DUSER_SETUP_LOADED, -DST7789_DRIVER, -DTFT_WIDTH, -DTFT_HEIGHT,
   -DTFT_MOSI, -DTFT_SCLK, -DTFT_CS, -DTFT_DC, -DTFT_RST, -DTFT_BL,
   -DLOAD_GLCD, -DLOAD_FONT2, -DLOAD_FONT4, -DSMOOTH_FONT, -DSPI_FREQUENCY
   (toute la config est desormais dans LGFX_Config.hpp)

## 2. Placer le fichier
Copier `include/LGFX_Config.hpp` dans le dossier `include/` du projet.

## 3. Adapter le code d'affichage
Remplacer :
    #include <TFT_eSPI.h>
    TFT_eSPI tft = TFT_eSPI();
Par :
    #include "LGFX_Config.hpp"
    LGFX tft;

Dans setup(), avant d'utiliser l'ecran :
    tft.init();
    tft.setRotation(0);      // 0..3 selon orientation voulue
    tft.setBrightness(255);  // retroeclairage (0..255) - remplace digitalWrite(TFT_BL,HIGH)
    tft.fillScreen(TFT_BLACK);

Le reste de l'API est quasi identique : tft.fillScreen, tft.drawString,
tft.setTextColor, tft.setCursor, tft.print, tft.drawRect, tft.fillRect...
Les constantes couleur (TFT_BLACK, TFT_WHITE, TFT_RED...) existent aussi.
Sprites : remplacer `TFT_eSprite spr(&tft);` par `LGFX_Sprite spr(&tft);`.

## 4. Calage de l'ecran 76x284 (offsets)
Le panneau (76x284) est plus petit que la GRAM du ST7789 (240x320) :
il faut decaler la zone visible. Valeurs de depart dans LGFX_Config.hpp :
    offset_x = 82   ((240-76)/2)
    offset_y = 18   ((320-284)/2)
Procedure de reglage :
1. tft.fillScreen(TFT_RED); puis dessiner un cadre :
   tft.drawRect(0,0, tft.width(), tft.height(), TFT_WHITE);
2. Si le cadre est decale/coupe -> ajuster offset_x / offset_y de +/-2
   jusqu'a ce que le cadre touche pile les 4 bords.
3. Si les couleurs sont inversees -> passer cfg.invert a false.
4. Si rouge et bleu sont echanges -> passer cfg.rgb_order a true.

## 5. Build
    pio run -e esp32-c6
Attendu : [SUCCESS]. Plus d'erreur gpio_out_w1tc / REG_SPI_BASE.

## Note env `native`
L'env de tests `native` echoue car `g++` (MinGW) n'est pas installe sur
le PC. Sans rapport avec l'ecran. Pour builder uniquement la cible reelle :
    pio run -e esp32-c6
