The C5 panel initialization sequence and RGB565-to-RGB666 conversion in
`firmware/src/board_c5_273.cpp` are adapted from Waveshare's
ESP32-C5-LCD-2.73 Arduino-v3.3.10 `07_example/esp_lcd_ili9488.c`.

The reference is provided under Apache License 2.0; its license is retained in
`WAVESHARE-LICENSE.txt`. Pirate Box uses a smaller bounded DMA tile, completion
callbacks, a PSRAM LVGL buffer, diagnostics and native shared-bus SD mounting.

Source: https://github.com/waveshareteam/ESP32-C5-LCD-2.73
