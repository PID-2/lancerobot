// board_ui.h — the two things the ESP32-C6-DevKitC-1 already has on board:
// the BOOT button on GPIO9 and the addressable RGB LED on GPIO8 (user guide).
// Shared by both hardware layers so the dry run has the same interface as the
// real mouse. On the desktop shim these fall through to stubs.

#pragma once
#include "config.h"

#if defined(ARDUINO_ARCH_ESP32)
  #include <Arduino.h>
  #if !defined(ESP_ARDUINO_VERSION) || ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    #error "The ESP32-C6 needs Arduino-ESP32 core 3.x (Boards Manager: esp32 by Espressif Systems)"
  #endif
#else
  #include "Arduino.h"   // desktop shim
  void shimLedWrite(uint8_t r, uint8_t g, uint8_t b);
#endif

inline void boardUiBegin() {
  pinMode(PIN_BUTTON, INPUT_PULLUP);
}

inline bool boardButtonPressed() {
  return digitalRead(PIN_BUTTON) == LOW;
}

// r, g, b in 0..255 before brightness scaling.
inline void boardLedWrite(uint8_t r, uint8_t g, uint8_t b) {
  const uint8_t sr = (uint8_t)((r * LED_BRIGHTNESS) / 255);
  const uint8_t sg = (uint8_t)((g * LED_BRIGHTNESS) / 255);
  const uint8_t sb = (uint8_t)((b * LED_BRIGHTNESS) / 255);
#if defined(ARDUINO_ARCH_ESP32)
  #if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 5)
    rgbLedWrite(PIN_STATUS_LED, sr, sg, sb);
  #else
    neopixelWrite(PIN_STATUS_LED, sr, sg, sb);
  #endif
#else
  shimLedWrite(sr, sg, sb);
#endif
}
