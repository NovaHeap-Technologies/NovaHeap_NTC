// NovaHeap_NTC: NTC thermistor temperature for Arduino, with open and short sensor detection.
// Copyright (c) 2026 NovaHeap Technologies
// Released under the NovaHeap Technologies License (MIT terms). See LICENSE.

#include "NovaHeap_NTC.h"

#include <math.h>

static const float RAIL = 0.01f;  // within 1% of a rail: open or shorted

// What each board's built-in ADC setup is like, until setReference() describes your own.
//   REFERENCE  ADC full scale
//   SUPPLY     the divider's supply, if it isn't the ADC reference (0: same). Where they are the
//              same rail, the reading is ratiometric: the exact voltage cancels out.
//   ADC_MIN/MAX  the range the ADC can tell apart from a pin pulled to GND or all the way up
//              (MAX 0: the reference)
//   LOAD       resistance from the pin to GND on the board itself (0: none)
//   MARGIN     how close to the highest voltage counts as open or shorted
#if defined(__AVR__) || defined(ARDUINO_ARCH_RENESAS_UNO)
static const float BOARD_REFERENCE = 5.0f;
static const float BOARD_SUPPLY = 0;
static const float BOARD_ADC_MIN = 0, BOARD_ADC_MAX = 0;
static const float BOARD_LOAD = 0, BOARD_MARGIN = RAIL;
#elif defined(ARDUINO_ARCH_ESP8266)
// NodeMCU and D1 mini: 220k + 100k from A0 to GND, in front of a 1 V ADC. It isn't ratiometric,
// so a 3.25 V rail instead of 3.3 V moves the reading: open and short detection needs more margin.
static const float BOARD_REFERENCE = 3.2f;
static const float BOARD_SUPPLY = 3.3f;
static const float BOARD_ADC_MIN = 0, BOARD_ADC_MAX = 0;
static const float BOARD_LOAD = 320000.0f, BOARD_MARGIN = 0.05f;
#elif defined(ARDUINO_ARCH_ESP32)
// analogReadMilliVolts(). The ADC flattens out well below 3.3 V, and a grounded pin still reads
// about 0.14 V.
static const float BOARD_REFERENCE = 3.3f;
static const float BOARD_SUPPLY = 3.3f;
static const float BOARD_ADC_MIN = 0.2f, BOARD_ADC_MAX = 2.9f;
static const float BOARD_LOAD = 0, BOARD_MARGIN = RAIL;
#else
static const float BOARD_REFERENCE = 3.3f;
static const float BOARD_SUPPLY = 0;
static const float BOARD_ADC_MIN = 0, BOARD_ADC_MAX = 0;
static const float BOARD_LOAD = 0, BOARD_MARGIN = RAIL;
#endif

static const unsigned long SAMPLE_MS = 100;  // time between readings
static const uint8_t SAMPLES = 8;            // ADC readings averaged into one
static const float KELVIN = 273.15f;

// Time since `since`, correct across the millis() rollover every 49.7 days. millis() is
// 32-bit on Arduino boards, so the subtraction is done in 32 bits even where
// unsigned long is 64-bit (such as a Linux PC running the unit tests).
static inline uint32_t elapsed(unsigned long now, unsigned long since) {
  return (uint32_t)(now - since);
}

NHThermistor::NHThermistor(uint8_t pin)
    : _pin(pin),
      _begun(false),
      _boardAdc(true),
      _toVcc(false),
      _voltsPerCount(BOARD_REFERENCE / 1024.0f),
      _reference(BOARD_REFERENCE),
      _supply(0),
      _series(10000),
      _a(0),
      _b(0),
      _c(0),
      _filterMs(1000),
      _sampledAt(0),
      _offset(0),
      _celsius(0),
      _resistance(0),
      _status(NH_NTC_OK) {
  setBeta(3950);
}

void NHThermistor::setSeriesResistor(float ohms) {
  if (!(ohms > 0)) return;
  _series = ohms;
  if (_begun) begin();
}

void NHThermistor::setWiring(NHNtcWiring wiring) {
  _toVcc = wiring == NH_NTC_TO_VCC;
  if (_begun) begin();
}

bool NHThermistor::setBeta(float beta, float nominalOhms, float nominalCelsius) {
  if (!(beta > 0) || !(nominalOhms > 0) || !(nominalCelsius > -KELVIN)) return false;
  // The Beta equation is Steinhart-Hart without the cubic term.
  _a = 1.0f / (nominalCelsius + KELVIN) - logf(nominalOhms) / beta;
  _b = 1.0f / beta;
  _c = 0;
  if (_begun) begin();
  return true;
}

void NHThermistor::setSteinhartHart(float a, float b, float c) {
  _a = a;
  _b = b;
  _c = c;
  if (_begun) begin();
}

bool NHThermistor::setThreePoints(float celsius1, float ohms1, float celsius2, float ohms2,
                                  float celsius3, float ohms3) {
  if (!(ohms1 > 0) || !(ohms2 > 0) || !(ohms3 > 0)) return false;
  if (!(celsius1 > -KELVIN) || !(celsius2 > -KELVIN) || !(celsius3 > -KELVIN)) return false;

  float l1 = logf(ohms1), l2 = logf(ohms2), l3 = logf(ohms3);
  float y1 = 1.0f / (celsius1 + KELVIN), y2 = 1.0f / (celsius2 + KELVIN),
        y3 = 1.0f / (celsius3 + KELVIN);
  if (l1 == l2 || l1 == l3 || l2 == l3 || l1 + l2 + l3 == 0) return false;

  float g2 = (y2 - y1) / (l2 - l1);
  float g3 = (y3 - y1) / (l3 - l1);
  float c = (g3 - g2) / (l3 - l2) / (l1 + l2 + l3);
  float b = g2 - c * (l1 * l1 + l1 * l2 + l2 * l2);
  float a = y1 - (b + c * l1 * l1) * l1;
  if (!(b > 0)) return false;  // resistance must fall as temperature rises

  setSteinhartHart(a, b, c);
  return true;
}

void NHThermistor::setReference(float volts, uint8_t bits) {
  if (!(volts > 0) || bits == 0 || bits > 16) return;
  _reference = volts;
  _voltsPerCount = volts / (float)(1UL << bits);
  _boardAdc = false;  // your own ADC setup, so none of the board's quirks
  if (_begun) begin();
}

void NHThermistor::setSupply(float volts) {
  if (!(volts > 0)) return;
  _supply = volts;
  if (_begun) begin();
}

// A setting changed after begin() starts over, as begin() does, so no reading taken the old way
// is blended into the new ones.
void NHThermistor::begin() {
  _begun = true;
  _sampledAt = millis();
  measure(SAMPLES * 4, 0);
}

void NHThermistor::ready() {
  if (!_begun) begin();
}

bool NHThermistor::update() {
  ready();
  unsigned long now = millis();
  uint32_t dt = elapsed(now, _sampledAt);
  if (dt < SAMPLE_MS) return false;
  _sampledAt = now;
  measure(SAMPLES, dt);
  return true;
}

// Take a reading. dt is the time since the last one, or 0 to start the smoothing afresh.
void NHThermistor::measure(uint8_t samples, uint32_t dt) {
  float pin = read(samples);
  float supply = _supply > 0 ? _supply : (_boardAdc && BOARD_SUPPLY > 0) ? BOARD_SUPPLY : _reference;
  float load = _boardAdc ? BOARD_LOAD : 0;
  float margin = _boardAdc ? BOARD_MARGIN : RAIL;

  // The highest and lowest the pin can go: with the thermistor open or shorted, or as far as the
  // ADC can show. At either, the thermistor is open (infinite resistance) or shorted (none).
  float high = (!_toVcc && load > 0) ? supply * load / (_series + load) : supply;
  float adcMax = (_boardAdc && BOARD_ADC_MAX > 0) ? BOARD_ADC_MAX : _reference;
  if (high > adcMax) high = adcMax;
  float low = supply * RAIL;
  if (_boardAdc && low < BOARD_ADC_MIN) low = BOARD_ADC_MIN;
  bool atTop = pin >= high * (1.0f - margin);
  bool atBottom = pin <= low;
  if (atTop || atBottom) {
    _status = (atTop != _toVcc) ? NH_NTC_OPEN : NH_NTC_SHORT;
    _resistance = NAN;
    return;
  }

  // The thermistor's resistance, taking out any load on the pin. Below `high`, the measured
  // resistance is less than the load, so the subtraction stays positive.
  float r = pin / supply;
  if (_toVcc) {
    float bottom = load > 0 ? _series * load / (_series + load) : _series;
    _resistance = bottom * (1.0f - r) / r;
  } else {
    float measured = _series * r / (1.0f - r);  // the thermistor in parallel with the load
    _resistance = load > 0 ? measured * load / (load - measured) : measured;
  }
  float lnR = logf(_resistance);
  float c = 1.0f / (_a + _b * lnR + _c * lnR * lnR * lnR) - KELVIN;

  if (_status != NH_NTC_OK || dt == 0 || _filterMs == 0) {
    _celsius = c;  // first good reading: start from it, not from an old value
  } else {
    // Exponential smoothing, weighted by the real time since the last reading, so the time
    // constant holds however often update() is called.
    _celsius += (c - _celsius) * (1.0f - expf(-(float)dt / (float)_filterMs));
  }
  _status = NH_NTC_OK;
}

float NHThermistor::celsius() {
  ready();
  return _status == NH_NTC_OK ? _celsius + _offset : NAN;
}

float NHThermistor::fahrenheit() { return celsius() * 1.8f + 32.0f; }

float NHThermistor::resistance() {
  ready();
  return _resistance;
}

NHNtcStatus NHThermistor::status() {
  ready();
  return _status;
}

// Pin volts, averaged over `samples` ADC readings.
float NHThermistor::read(uint8_t samples) {
  // The first reading after the ADC switches pins can be off. Throw it away.
  analogRead(_pin);

  uint32_t sum = 0;
#if defined(ARDUINO_ARCH_ESP32)
  if (_boardAdc) {
    for (uint8_t i = 0; i < samples; i++) sum += analogReadMilliVolts(_pin);
    return (float)sum / samples * 0.001f;
  }
#endif
  for (uint8_t i = 0; i < samples; i++) sum += analogRead(_pin);
  return (float)sum / samples * _voltsPerCount;
}
