/*
  03_DatasheetCurve: the most accurate setup, from your thermistor's datasheet table.

  The Beta value describes a thermistor well near room temperature, but drifts by a degree
  or more towards the ends of its range. Datasheets list the resistance at each temperature.
  Pick three rows (one cold, one in the middle, one hot) and this library fits the
  Steinhart-Hart equation through them.

  Choose the rows to span the temperatures you care about. The values below are only an
  example: copy yours from the datasheet.

  If the datasheet gives Steinhart-Hart coefficients directly, use them instead:
      ntc.setSteinhartHart(1.129148e-03, 2.34125e-04, 8.76741e-08);

  Wiring:  5V (3.3V on a 3.3 V board) -> 10k resistor -> A0 -> thermistor -> GND

  NovaHeap_NTC by NovaHeap Technologies
*/

#include <NovaHeap_NTC.h>

NHThermistor ntc(A0);

void setup() {
  Serial.begin(9600);
  while (!Serial && millis() < 3000) {}  // boards with native USB: wait for the Serial Monitor

  //                         deg C   ohms    deg C  ohms    deg C  ohms
  bool fitted = ntc.setThreePoints(-20, 97070, 25, 10000, 85, 1070);
  if (!fitted) {
    Serial.println("Those three points don't describe an NTC thermistor: check the numbers");
  }
  ntc.begin();
}

void loop() {
  ntc.update();

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= 1000) {
    lastPrint = millis();
    if (ntc.isOk()) {
      Serial.print(ntc.celsius(), 2);
      Serial.print(" C  (");
      Serial.print(ntc.resistance(), 0);
      Serial.println(" ohms)");
    } else {
      Serial.println("Sensor fault: check the wiring");
    }
  }
}
