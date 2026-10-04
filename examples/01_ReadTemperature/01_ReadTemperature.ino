/*
  01_ReadTemperature: print the temperature every second.

  For the common "10k B3950" NTC thermistor. Most cheap probes and beads are this type.

  Wiring:  5V (3.3V on a 3.3 V board) -> 10k resistor -> A0 -> thermistor -> GND

  Feed the divider from the board's own supply (5V or 3.3V), not from a separate one, so
  the reading doesn't depend on its exact voltage.

  If the reading falls when you warm the thermistor, it is on the other side of the
  divider (some modules, like the KY-013, are wired that way). Add this to setup():
      ntc.setWiring(NH_NTC_TO_VCC);

  NovaHeap_NTC by NovaHeap Technologies
*/

#include <NovaHeap_NTC.h>

NHThermistor ntc(A0);

void setup() {
  Serial.begin(9600);
  // ntc.setBeta(3435);  // a different thermistor: its Beta value, from the label or datasheet
  ntc.begin();
}

void loop() {
  ntc.update();  // takes a reading every 100 ms and smooths it

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= 1000) {
    lastPrint = millis();

    if (ntc.status() == NH_NTC_OPEN) {
      Serial.println("No thermistor: check the wiring");
    } else if (ntc.status() == NH_NTC_SHORT) {
      Serial.println("Thermistor shorted: check the wiring");
    } else {
      Serial.print(ntc.celsius(), 1);
      Serial.print(" C  ");
      Serial.print(ntc.fahrenheit(), 1);
      Serial.println(" F");
    }
  }
}
