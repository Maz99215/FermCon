# FermCon — Câblage Wokwi / matériel réel

Câblage détaillé :
- Display ST7789 (substitué par ILI9341 dans Wokwi) :
  - VCC -> 3V3
  - GND -> GND
  - SCK -> GPIO6
  - MOSI -> GPIO7
  - CS -> GPIO10
  - DC -> GPIO11
  - RST -> GPIO21
  - BL -> GPIO22
- Capteur DS18B20 :
  - DQ -> GPIO4
  - VCC -> 3V3
  - GND -> GND
  - Résistance pull-up 4.7k entre DQ et 3V3
- SSR Froid (GPIO2) :
  - Anode via résistance 220 -> GPIO2
  - Cathode -> GND
- Relais Chaud (GPIO3) :
  - Anode via résistance 220 -> 3V3
  - Cathode -> GPIO3
