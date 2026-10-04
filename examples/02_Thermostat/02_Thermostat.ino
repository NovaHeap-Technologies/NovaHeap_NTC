/*
  02_Thermostat: keep something warm, and fail safe if the sensor breaks.

  Heats below 37.0 C and stops at 38.0 C. The gap stops the heater switching on and off
  around one value. If the thermistor comes loose or shorts, the heater goes off: a
  thermostat that keeps heating on a broken sensor can start a fire.

  Wiring:  5V (3.3V on a 3.3 V board) -> 10k resistor -> A0 -> thermistor -> GND
           pin 7 (GPIO 4 on ESP32 and ESP8266) -> relay module IN, or a logic-level
           MOSFET gate, switching the heater

  For relay modules, NovaHeap_Relay adds a click-free start-up and minimum on/off times.
  Mains heaters need a relay rated for the load, a fuse, and an enclosure.

  NovaHeap_NTC by NovaHeap Technologies
*/

#include <NovaHeap_NTC.h>

#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
const int HEATER_PIN = 4;  // GPIO 6 to 11 run the flash chip, and GPIO 5 pulses at boot
#else
const int HEATER_PIN = 7;
#endif
const uint8_t HEATER_ON = HIGH;  // swap these two for an active-LOW relay module
const uint8_t HEATER_OFF = LOW;

const float HEAT_BELOW = 37.0;  // degrees C
const float STOP_AT = 38.0;

NHThermistor ntc(A0);
bool heating = false;

void setup() {
  Serial.begin(9600);
  pinMode(HEATER_PIN, OUTPUT);
  digitalWrite(HEATER_PIN, HEATER_OFF);
  ntc.begin();
}

void loop() {
  ntc.update();

  if (!ntc.isOk()) {
    heating = false;  // fail safe
  } else if (ntc.celsius() < HEAT_BELOW) {
    heating = true;
  } else if (ntc.celsius() >= STOP_AT) {
    heating = false;
  }
  digitalWrite(HEATER_PIN, heating ? HEATER_ON : HEATER_OFF);

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= 1000) {
    lastPrint = millis();
    if (ntc.isOk()) {
      Serial.print(ntc.celsius(), 1);
      Serial.print(" C  heater ");
      Serial.println(heating ? "ON" : "off");
    } else {
      Serial.println("Sensor fault: heater off");
    }
  }
}
