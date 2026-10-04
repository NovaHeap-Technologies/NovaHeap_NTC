// NovaHeap_NTC: NTC thermistor temperature for Arduino, with open and short sensor detection.
// Copyright (c) 2026 NovaHeap Technologies
// Released under the NovaHeap Technologies License (MIT terms). See LICENSE.

#ifndef NOVAHEAP_NTC_H
#define NOVAHEAP_NTC_H

#include <Arduino.h>

// Where the thermistor sits in the divider.
enum NHNtcWiring : uint8_t {
  NH_NTC_TO_GND,  // supply -> fixed resistor -> pin -> thermistor -> GND (the default)
  NH_NTC_TO_VCC   // supply -> thermistor -> pin -> fixed resistor -> GND
};

enum NHNtcStatus : uint8_t {
  NH_NTC_OK,
  NH_NTC_OPEN,  // thermistor missing or a wire broken
  NH_NTC_SHORT  // thermistor shorted
};

class NHThermistor {
 public:
  explicit NHThermistor(uint8_t pin);

  // Setup. Call these before begin(), in any order. The defaults suit the common
  // "10k B3950" thermistor with a 10k fixed resistor, thermistor to GND.

  void setSeriesResistor(float ohms);  // the fixed resistor in the divider
  void setWiring(NHNtcWiring wiring);

  // The thermistor, by its Beta value: "10k B3950" is setBeta(3950, 10000).
  // Returns false, and changes nothing, if beta or nominalOhms isn't above zero.
  bool setBeta(float beta, float nominalOhms = 10000, float nominalCelsius = 25);

  // The thermistor, by Steinhart-Hart coefficients: 1/T = a + b ln(R) + c ln(R)^3, T in kelvin.
  void setSteinhartHart(float a, float b, float c);

  // The thermistor, by three points from its datasheet table: temperature and resistance at a
  // low, middle and high temperature. Returns false, and changes nothing, if they don't fit.
  bool setThreePoints(float celsius1, float ohms1, float celsius2, float ohms2, float celsius3,
                      float ohms3);

  // The ADC's full-scale voltage and resolution, as in NovaHeap_BatteryMonitor. Only boards
  // where the divider's supply isn't the ADC reference care: see the README.
  void setReference(float volts, uint8_t bits = 10);

  // The voltage feeding the divider, if it isn't the ADC reference.
  void setSupply(float volts);

  // Smoothing time constant. Default 1000 ms; 0 turns smoothing off.
  void setFilterTime(unsigned long ms) { _filterMs = ms; }

  // Added to every reading, to correct a sensor that reads high or low.
  void setOffset(float celsius) { _offset = celsius; }

  // Call once in setup(). Takes the first reading.
  void begin();

  // Call every loop(). Takes a new reading every 100 ms; returns true when it did.
  bool update();

  float celsius();     // smoothed temperature; NAN while the sensor is open or shorted
  float fahrenheit();  // the same, in Fahrenheit
  float resistance();  // the thermistor's resistance at the latest reading, in ohms
  NHNtcStatus status();
  bool isOk() { return status() == NH_NTC_OK; }

 private:
  void ready();
  float read(uint8_t samples);
  void measure(uint8_t samples, uint32_t dt);

  uint8_t _pin;
  bool _begun;
  bool _boardAdc;  // the board's own ADC setup, until setReference()
  bool _toVcc;
  float _voltsPerCount;
  float _reference;  // ADC full scale
  float _supply;     // from setSupply(), or 0
  float _series;
  float _a, _b, _c;  // Steinhart-Hart

  unsigned long _filterMs;
  unsigned long _sampledAt;
  float _offset;
  float _celsius;
  float _resistance;
  NHNtcStatus _status;
};

#endif
