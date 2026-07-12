// =============================================================================
//  FermCon - Configuration LovyanGFX pour ESP32-C6 + ecran ST7789 2.25" 76x284 SPI
//  Ecran "barre" DIYUSER 2.25" - controleur ST7789, 4-wire SPI, write-only.
//  Cablage module :
//    GND->GND | VCC->3V3 | SCL(SCLK)->6 | SDA(MOSI)->7 | RST->21 | DC->11 | CS->10 | BL->22
//  Bus : SPI2_HOST, 40 MHz. Pas de MISO (spi_3wire = true).
//
//  NB retroeclairage : le pin BL (GPIO 22) est gere par DisplayManager
//  (ledcAttach + setBacklight()), PAS par LovyanGFX -> pas de bloc Light ici,
//  sinon conflit ledc sur le meme pin.
//
//  IMPORTANT - panneau 76x284 sur GRAM ST7789 240x320 => OFFSETS obligatoires.
//  Valeurs de depart : offset_x=82, offset_y=18. A AJUSTER si l'image est
//  decalee ou rognee (voir procedure de calage dans le README).
// =============================================================================
#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device
{
  lgfx::Panel_ST7789  _panel_instance;
  lgfx::Bus_SPI       _bus_instance;

public:
  LGFX(void)
  {
    // ---- Bus SPI ------------------------------------------------------------
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host    = SPI2_HOST;   // ESP32-C6 : SPI2_HOST (FSPI)
      cfg.spi_mode    = 0;
      cfg.freq_write  = 40000000;    // 40 MHz
      cfg.freq_read   = 16000000;
      cfg.spi_3wire   = true;        // pas de MISO
      cfg.use_lock    = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk    = 6;
      cfg.pin_mosi    = 7;
      cfg.pin_miso    = -1;
      cfg.pin_dc      = 11;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    // ---- Panneau ST7789 76x284 ---------------------------------------------
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs           = 10;
      cfg.pin_rst          = 21;
      cfg.pin_busy         = -1;

      cfg.memory_width     = 240;   // GRAM physique du ST7789
      cfg.memory_height    = 320;
      cfg.panel_width      = 76;    // zone visible reelle
      cfg.panel_height     = 284;
      cfg.offset_x         = 82;    // (240-76)/2  -> a caler
      cfg.offset_y         = 18;    // (320-284)/2 -> a caler
      cfg.offset_rotation  = 0;

      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable         = false;
      cfg.invert           = true;   // false si couleurs inversees
      cfg.rgb_order        = false;  // true si rouge/bleu echanges
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = false;
      _panel_instance.config(cfg);
    }

    setPanel(&_panel_instance);
  }
};
