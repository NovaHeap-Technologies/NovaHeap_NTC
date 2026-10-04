# NovaHeap_NTC

NTC thermistor temperature for Arduino, with open and shorted sensor detection, by NovaHeap Technologies.

[![CI](https://github.com/NovaHeap-Technologies/NovaHeap_NTC/actions/workflows/ci.yml/badge.svg)](https://github.com/NovaHeap-Technologies/NovaHeap_NTC/actions/workflows/ci.yml)

Thermistors are cheap, tough and accurate, but turning one into a temperature means a divider formula, a logarithm and a curve. Then a loose wire quietly reads −40 °C or 300 °C, and a thermostat acts on it. NovaHeap_NTC does the maths, and tells you when the sensor isn't there.

- **Works out of the box** with the common 10k B3950 thermistor and a 10k resistor.
- **Any thermistor.** Set its Beta value, Steinhart-Hart coefficients, or three rows of its datasheet table.
- **Detects open and shorted sensors.** `celsius()` returns `NAN` instead of a wrong number, and `status()` says which fault it is.
- **Ratiometric.** With the divider fed from the board's own supply, the exact supply voltage cancels out.
- **Steady readings.** Averaged and smoothed, without `delay()`.
- **Runs on any board.** No dynamic memory, timers or interrupts.

## Install

**Arduino IDE:** Tools → Manage Libraries, search for **NovaHeap NTC**, click Install.

**Manually:** download this repository as a ZIP, then Sketch → Include Library → Add .ZIP Library.

## Wiring

```
5V (3.3V on 3.3 V boards) ──[ 10k ]──┬── A0
                                     │
                                  [ NTC ]
                                     │
GND ─────────────────────────────────┘
```

Feed the divider from the board's own 5V or 3.3V pin, the same voltage as its ADC reference. Then the reading doesn't depend on that voltage at all.

The thermistor can also go on top (`setWiring(NH_NTC_TO_VCC)`). Some modules, like the KY-013, are wired that way. If the temperature falls when you warm the sensor, that's the setting to change.

## Quick start

```cpp
#include <NovaHeap_NTC.h>

NHThermistor ntc(A0);

void setup() {
  Serial.begin(9600);
  ntc.begin();
}

void loop() {
  ntc.update();  // call every loop

  if (ntc.isOk()) {
    Serial.print(ntc.celsius(), 1);
    Serial.println(" C");
  } else {
    Serial.println("Sensor fault");
  }
  delay(1000);
}
```

## Choosing a thermistor model

| Your thermistor's data | Use | Accuracy |
|---|---|---|
| A name like "10k B3950" or "NTC 100K 3950" | `setBeta(3950)` for 10k (the default), `setBeta(3950, 100000)` for 100k | Good near 25 °C; drifts by a degree or more far from it |
| Datasheet resistance table | `setThreePoints(-20, 97070, 25, 10000, 85, 1070)` | Best across the three points' range |
| Steinhart-Hart coefficients | `setSteinhartHart(a, b, c)` | Best |

For `setThreePoints()`, pick a cold, a middle and a hot row spanning the temperatures you need.

## Examples

| Example | Shows |
|---|---|
| `01_ReadTemperature` | Celsius and Fahrenheit every second, with fault messages |
| `02_Thermostat` | A heater with hysteresis that switches off if the sensor fails |
| `03_DatasheetCurve` | Three points from a datasheet table, for the best accuracy |
| `04_100kThermistor` | A 3D-printer style 100k thermistor with a 4.7k resistor, up to 250 °C |

## Reference

| Call | Does |
|---|---|
| `NHThermistor t(pin)` | Create a thermistor on an analog pin |
| `t.setSeriesResistor(ohms)` | The fixed resistor. Default 10 kΩ |
| `t.setWiring(NH_NTC_TO_VCC)` | Thermistor on top of the divider. Default `NH_NTC_TO_GND` |
| `t.setBeta(beta, ohms, celsius)` | Beta model. `ohms` at `celsius` default to 10 kΩ at 25 °C. Returns `false` if invalid |
| `t.setSteinhartHart(a, b, c)` | Steinhart-Hart model, temperature in kelvin |
| `t.setThreePoints(c1, r1, c2, r2, c3, r3)` | Fit Steinhart-Hart through three datasheet rows. Returns `false` if they don't describe an NTC |
| `t.setReference(volts, bits)` | ADC full scale, if the default doesn't suit (see below) |
| `t.setSupply(volts)` | The divider's supply, if it isn't the ADC reference |
| `t.setFilterTime(ms)` | Smoothing time constant. Default 1000 ms; 0 = off |
| `t.setOffset(celsius)` | Added to every reading, to correct a sensor that reads high or low |
| `t.begin()` | Take the first reading. Call once in `setup()` |
| `t.update()` | Call every `loop()`. Reads every 100 ms; returns `true` when it did |
| `t.celsius()` / `t.fahrenheit()` | Smoothed temperature, or `NAN` while the sensor is faulty |
| `t.resistance()` | Thermistor resistance at the latest reading, or `NAN` while faulty |
| `t.status()` | `NH_NTC_OK`, `NH_NTC_OPEN` or `NH_NTC_SHORT` |
| `t.isOk()` | `true` when the status is `NH_NTC_OK` |

### ADC defaults

| Board | Default | Notes |
|---|---|---|
| Uno, Nano, Mega, Uno R4 | 5 V, 10-bit, ratiometric | Feed the divider from 5V |
| ESP32 | `analogReadMilliVolts()`, divider on 3.3 V | See the ESP32 note below |
| ESP8266 (NodeMCU, D1 mini) | 3.2 V full scale, divider on 3.3 V | Allows for the board's own 220k + 100k divider on A0, and for a 3.2–3.3 V rail. With a 10k B3950 to GND and a 10k resistor, below about −29 °C reads as open. Bare ESP-12 modules read only 0–1 V on the ADC pin: use a NodeMCU or D1 mini |
| Everything else (SAMD, RP2040, ...) | 3.3 V, 10-bit, ratiometric | If you call `analogReadResolution(12)`, also call `setReference(3.3, 12)` |

**ESP32:** its ADC is accurate only between about 0.2 V and 2.45 V. Above 2.9 V or below 0.2 V at the pin, a reading counts as open or shorted. With a 10k B3950 and a 10k resistor:

| Wiring | Accurate | Reads as a fault |
|---|---|---|
| `NH_NTC_TO_GND` (default) | about +3 to +100 °C | below about −12 °C, above about +103 °C |
| `NH_NTC_TO_VCC` | about −26 to +50 °C | below about −26 °C, above about +75 °C |

Pick the wiring that covers your temperatures.

### How readings and faults behave

- `update()` reads every 100 ms. It throws away one ADC reading, averages the next eight, and smooths the temperature. Smoothing uses the real time between readings, so the time constant holds however often `loop()` runs.
- A pin within 1% of either rail means the thermistor is open or shorted (ESP32 and ESP8266 need wider limits; see above). `setReference()` means you're describing your own ADC setup, so it turns those board limits off. While it is, `celsius()` returns `NAN`, which compares false with everything: `if (ntc.celsius() > 30)` won't switch anything on. Check `isOk()` first to switch things off.
- When the sensor comes back, the reading starts afresh from it, with no blending in old values.
- Changing the thermistor, resistor, wiring, reference or supply after `begin()` starts the readings over, as `begin()` does.

## Accuracy

- **Use a 1% fixed resistor.** A thermistor changes about 4% per °C, so a 5% resistor alone can be a degree out. A 1% thermistor and a 1% resistor are within about half a degree near 25 °C.
- **Match the resistor to your temperatures.** The reading is most precise where the thermistor's resistance equals the fixed resistor. At 10-bit, a 10k/10k divider resolves about 0.1 °C near 25 °C, but only about 0.5 °C at 100 °C. For hot readings use a smaller resistor (see `04_100kThermistor`).
- **Self-heating.** The divider current warms the thermistor slightly, by up to about half a degree in still air at 5 V. For precise work use a larger thermistor and resistor (100k), or correct it with `setOffset()`.
- **Long wires pick up noise.** Add 100 nF from the analog pin to GND.

## Tests

The conversion, fault detection and smoothing have unit tests that run on a PC against a fake Arduino:

```
make -C extras/test
```

GitHub Actions runs them on every push, lints the library, and compiles the examples for AVR, Uno R4, SAMD, ESP32, ESP8266 and RP2040 boards.

## License

NovaHeap Technologies License (MIT terms): free for personal and commercial use, as long as the copyright notice stays in. See [LICENSE](LICENSE).

---

Made by **NovaHeap Technologies**. Pairs with [NovaHeap_Relay](https://github.com/NovaHeap-Technologies/NovaHeap_Relay) for switching heaters and fans.
