# Changelog

## 1.0.0

First release.

- `NHThermistor`: NTC temperature in Celsius and Fahrenheit, and the thermistor's resistance
- Beta equation (default 10k B3950), Steinhart-Hart coefficients, or a fit through three datasheet points
- Thermistor to GND or to VCC; any fixed resistor
- Ratiometric reading, averaged and smoothed; factory-calibrated `analogReadMilliVolts()` on ESP32
- Open and shorted sensor detection: `status()`, `isOk()`, and `NAN` instead of a wrong temperature
- Four examples, and unit tests that run on a PC
