// Minimal Arduino API for running the unit tests on a PC.
#pragma once
#include <stdint.h>

unsigned long millis();
int analogRead(uint8_t pin);
#if defined(ARDUINO_ARCH_ESP32)
uint32_t analogReadMilliVolts(uint8_t pin);
#endif
