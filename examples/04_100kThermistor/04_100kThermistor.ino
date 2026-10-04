/*
  04_100kThermistor: a 100k thermistor reading up to 250 C.

  3D printer hot ends and beds use 100k B3950 thermistors with a 4.7k pull-up resistor.
  The smaller resistor keeps the reading accurate at high temperatures, where the
  thermistor's resistance drops to a few hundred ohms.

  Wiring:  5V (3.3V on a 3.3 V board) -> 4.7k resistor -> A0 -> thermistor -> GND

  With the 4.7k resistor, anything below about -6 C reads as disconnected. That's fine for a
  hot end; for cold readings use a 100k resistor instead.

  Use a thermistor rated for the temperature: plain epoxy beads are good to about 125 C,
  and glass-bead hot end thermistors to about 300 C.

  NovaHeap_NTC by NovaHeap Technologies
*/

#include <NovaHeap_NTC.h>

NHThermistor hotEnd(A0);

void setup() {
  Serial.begin(9600);
  hotEnd.setBeta(3950, 100000);  // Beta, and resistance at 25 C
  hotEnd.setSeriesResistor(4700);
  hotEnd.setFilterTime(500);     // follow fast heating closely
  hotEnd.begin();
}

void loop() {
  hotEnd.update();

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= 1000) {
    lastPrint = millis();
    if (hotEnd.isOk()) {
      Serial.print(hotEnd.celsius(), 1);
      Serial.println(" C");
    } else {
      Serial.println(hotEnd.status() == NH_NTC_OPEN ? "Thermistor disconnected" : "Thermistor shorted");
    }
  }
}
