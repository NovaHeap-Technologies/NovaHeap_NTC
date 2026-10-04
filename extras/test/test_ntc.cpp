// Unit tests for NovaHeap_NTC, run on a PC against a fake Arduino.
// Build and run: make -C extras/test

#include <math.h>
#include <stdio.h>

#include "NovaHeap_NTC.h"

// ---- Fake hardware ---------------------------------------------------------

static const int PINS = 32;
static unsigned long fakeNow;
static int adc[PINS];  // what analogRead() returns on each pin
static int lastPin;    // the ADC returns garbage on the first read after switching pins

unsigned long millis() { return fakeNow; }

#if defined(ARDUINO_ARCH_ESP32)
static uint32_t mv[PINS];  // what analogReadMilliVolts() returns
uint32_t analogReadMilliVolts(uint8_t pin) { return mv[pin]; }
#endif

int analogRead(uint8_t pin) {
  if (pin != lastPin) {
    lastPin = pin;
    return 12345;
  }
  return adc[pin];
}

static void reset(unsigned long now = 0) {
  fakeNow = now;
  for (int p = 0; p < PINS; p++) adc[p] = 0;
#if defined(ARDUINO_ARCH_ESP32)
  for (int p = 0; p < PINS; p++) mv[p] = 0;
#endif
  lastPin = -1;
}

static void at(unsigned long now) { fakeNow = now; }

// Tests use a 16-bit ADC whose reference also feeds the divider, so the count is the pin
// voltage as a fraction of 65536.
static void precise(NHThermistor &t) {
  t.setReference(65.536f, 16);
  t.setFilterTime(0);
}

static void pinRatio(int pin, double r) { adc[pin] = (int)lround(r * 65536); }

// Pin ratio for a thermistor of `ohms` with a series resistor, either wiring.
static double ratio(double ohms, double series = 10000, bool toGnd = true) {
  return toGnd ? ohms / (ohms + series) : series / (ohms + series);
}

// Reference models, in double precision.
static double betaOhms(double c, double beta = 3950, double r0 = 10000, double c0 = 25) {
  return r0 * exp(beta * (1 / (c + 273.15) - 1 / (c0 + 273.15)));
}

static const double SH_A = 1.009249522e-03, SH_B = 2.378405444e-04, SH_C = 2.019202697e-07;

static double shOhms(double c) {  // the inverse of Steinhart-Hart
  double y = (SH_A - 1 / (c + 273.15)) / SH_C;
  double x = sqrt(pow(SH_B / (3 * SH_C), 3) + y * y / 4);
  return exp(cbrt(x - y / 2) - cbrt(x + y / 2));
}

// ---- Test runner -----------------------------------------------------------

static int checks, failures;

#define CHECK(cond)                                                   \
  do {                                                                \
    checks++;                                                         \
    if (!(cond)) {                                                    \
      failures++;                                                     \
      printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);          \
    }                                                                 \
  } while (0)

#define NEAR(a, b, tol) (fabs((double)(a) - (double)(b)) <= (tol))
#define IS_NAN(x) ((x) != (x))

// ---- Tests -----------------------------------------------------------------

#if !defined(ARDUINO_ARCH_ESP32) && !defined(ARDUINO_ARCH_ESP8266)
static void defaultReads25AtHalfScale() {
  reset();
  adc[1] = 512;  // default 3.3 V, 10-bit: half of the supply
  NHThermistor t(1);
  t.begin();
  CHECK(NEAR(t.celsius(), 25.0, 0.01));
  CHECK(NEAR(t.resistance(), 10000, 1));
  CHECK(t.isOk() && t.status() == NH_NTC_OK);
}
#endif

static void betaAcrossTheRange() {
  const double temps[] = {-30, 0, 25, 50, 100};
  for (double c : temps) {
    reset();
    NHThermistor t(1);
    precise(t);
    pinRatio(1, ratio(betaOhms(c)));
    t.begin();
    CHECK(NEAR(t.celsius(), c, 0.02));
    CHECK(NEAR(t.resistance(), betaOhms(c), betaOhms(c) * 0.0005));
  }
}

static void hundredKThermistor() {
  // A 3D-printer style 100k B3950 thermistor with a 4.7k pull-up.
  const double temps[] = {25, 100, 200, 250};
  for (double c : temps) {
    reset();
    NHThermistor t(1);
    precise(t);
    CHECK(t.setBeta(3950, 100000));
    t.setSeriesResistor(4700);
    pinRatio(1, ratio(betaOhms(c, 3950, 100000), 4700));
    t.begin();
    CHECK(NEAR(t.celsius(), c, 0.05));
  }
}

static void wiringToVcc() {
  reset();
  NHThermistor t(1);
  precise(t);
  t.setWiring(NH_NTC_TO_VCC);
  pinRatio(1, ratio(betaOhms(0), 10000, false));
  t.begin();
  CHECK(NEAR(t.celsius(), 0, 0.02));

  t.setWiring(NH_NTC_TO_GND);  // the same pin voltage now means hot
  CHECK(t.celsius() > 40);
}

static void openAndShortWithNtcToGnd() {
  reset();
  NHThermistor t(1);
  t.setReference(65.536f, 16);  // smoothing on: recovery must not blend with old readings
  pinRatio(1, 1.0);
  t.begin();
  CHECK(t.status() == NH_NTC_OPEN && !t.isOk());
  CHECK(IS_NAN(t.celsius()) && IS_NAN(t.fahrenheit()) && IS_NAN(t.resistance()));

  pinRatio(1, 0.0);
  at(100);
  t.update();
  CHECK(t.status() == NH_NTC_SHORT && IS_NAN(t.celsius()));

  pinRatio(1, 0.5);
  at(200);
  t.update();
  CHECK(t.isOk());
  CHECK(NEAR(t.celsius(), 25.0, 0.01));
}

static void openAndShortWithNtcToVcc() {
  reset();
  NHThermistor t(1);
  precise(t);
  t.setWiring(NH_NTC_TO_VCC);
  pinRatio(1, 1.0);
  t.begin();
  CHECK(t.status() == NH_NTC_SHORT);
  pinRatio(1, 0.0);
  t.begin();
  CHECK(t.status() == NH_NTC_OPEN);
}

static void railThresholds() {
  struct {
    double r;
    NHNtcStatus status;
  } cases[] = {{0.995, NH_NTC_OPEN}, {0.985, NH_NTC_OK}, {0.015, NH_NTC_OK}, {0.005, NH_NTC_SHORT}};
  for (auto c : cases) {
    reset();
    NHThermistor t(1);
    precise(t);
    pinRatio(1, c.r);
    t.begin();
    CHECK(t.status() == c.status);
  }
}

static void separateSupply() {
  // ESP8266 boards: the ADC tops out at 3.2 V but the divider is fed from 3.3 V.
  reset();
  NHThermistor t(1);
  t.setReference(3.2f, 10);
  t.setSupply(3.3f);
  t.setFilterTime(0);
  adc[1] = 528;  // 1.65 V: half of 3.3 V
  t.begin();
  CHECK(NEAR(t.celsius(), 25.0, 0.01));
  adc[1] = 1023;  // pulled up to 3.3 V, which the ADC sees as its 3.2 V maximum
  t.begin();
  CHECK(t.status() == NH_NTC_OPEN);
}

static void steinhartHart() {
  const double temps[] = {-20, 0, 25, 60, 100};
  for (double c : temps) {
    reset();
    NHThermistor t(1);
    precise(t);
    t.setSteinhartHart(SH_A, SH_B, SH_C);
    pinRatio(1, ratio(shOhms(c)));
    t.begin();
    CHECK(NEAR(t.celsius(), c, 0.02));
  }
}

static void threePointsFromDatasheet() {
  reset();
  NHThermistor t(1);
  precise(t);
  CHECK(t.setThreePoints(-20, shOhms(-20), 25, shOhms(25), 85, shOhms(85)));
  const double temps[] = {-20, 0, 25, 50, 85};
  for (double c : temps) {
    pinRatio(1, ratio(shOhms(c)));
    t.begin();
    CHECK(NEAR(t.celsius(), c, 0.05));
  }

  // Three points off a Beta curve give that curve back.
  CHECK(t.setThreePoints(0, betaOhms(0), 25, betaOhms(25), 50, betaOhms(50)));
  pinRatio(1, ratio(betaOhms(37)));
  t.begin();
  CHECK(NEAR(t.celsius(), 37, 0.05));
}

static void badCurvesAreRejected() {
  reset();
  NHThermistor t(1);
  precise(t);
  CHECK(!t.setBeta(0));
  CHECK(!t.setBeta(3950, 0));
  CHECK(!t.setBeta(3950, 10000, -300));
  CHECK(!t.setThreePoints(0, 10000, 25, 10000, 50, 3000));        // two equal resistances
  CHECK(!t.setThreePoints(0, 0, 25, 10000, 50, 3000));            // zero ohms
  CHECK(!t.setThreePoints(-300, 30000, 25, 10000, 50, 3000));     // below absolute zero
  CHECK(!t.setThreePoints(0, 3000, 25, 10000, 50, 30000));        // rises with temperature: PTC
  pinRatio(1, 0.5);
  t.begin();
  CHECK(NEAR(t.celsius(), 25.0, 0.01));  // still the default 10k B3950
}

static void invalidSettingsAreIgnored() {
  reset();
  adc[1] = 512;
  NHThermistor t(1);
  t.setReference(3.3f, 10);
  t.setSeriesResistor(0);
  t.setSeriesResistor(-1);
  t.setSupply(0);
  t.setReference(0, 10);
  t.setReference(3.3f, 0);
  t.setReference(3.3f, 17);
  t.begin();
  CHECK(NEAR(t.celsius(), 25.0, 0.01));
}

static void offsetAndFahrenheit() {
  reset();
  NHThermistor t(1);
  precise(t);
  pinRatio(1, 0.5);
  t.setOffset(-0.5f);
  t.begin();
  CHECK(NEAR(t.celsius(), 24.5, 0.01));
  CHECK(NEAR(t.fahrenheit(), 76.1, 0.02));
}

static void readsOnlyEvery100ms() {
  reset();
  NHThermistor t(1);
  precise(t);
  pinRatio(1, 0.5);
  t.begin();
  pinRatio(1, ratio(betaOhms(30)));
  at(99);
  CHECK(!t.update());
  CHECK(NEAR(t.celsius(), 25, 0.01));
  at(100);
  CHECK(t.update());
  CHECK(NEAR(t.celsius(), 30, 0.02));
  CHECK(!t.update());
}

static void filterTimeConstant() {
  // After one time constant the reading has moved 63% of a step, whether update() ran every
  // 100 ms or only once.
  for (int step = 100; step <= 1000; step += 900) {
    reset();
    NHThermistor t(1);
    t.setReference(65.536f, 16);
    t.setFilterTime(1000);
    pinRatio(1, ratio(betaOhms(25)));
    t.begin();
    pinRatio(1, ratio(betaOhms(35)));
    for (int ms = step; ms <= 1000; ms += step) {
      at(ms);
      t.update();
    }
    CHECK(NEAR(t.celsius(), 25 + 6.32, 0.03));
    for (int ms = 1000 + step; ms <= 6000; ms += step) {
      at(ms);
      t.update();
    }
    CHECK(NEAR(t.celsius(), 35, 0.05));
  }
}

static void settingsAfterBeginApplyAtOnce() {
  reset();
  NHThermistor t(1);
  t.setReference(65.536f, 16);  // smoothing on: a slow drift would show here
  pinRatio(1, ratio(betaOhms(60, 3950, 100000), 4700));
  t.begin();
  CHECK(t.setBeta(3950, 100000));
  t.setSeriesResistor(4700);
  CHECK(NEAR(t.celsius(), 60, 0.05));
}

static void switchingPinsDoesNotSpoilReadings() {
  reset();
  NHThermistor a(1), b(2);
  precise(a);
  precise(b);
  pinRatio(1, ratio(betaOhms(20)));
  pinRatio(2, ratio(betaOhms(40)));
  a.begin();
  b.begin();
  at(100);
  a.update();
  b.update();
  CHECK(NEAR(a.celsius(), 20, 0.02));
  CHECK(NEAR(b.celsius(), 40, 0.02));
}

static void worksWithoutBegin() {
  reset();
  NHThermistor t(1);
  precise(t);
  pinRatio(1, 0.5);
  CHECK(NEAR(t.celsius(), 25, 0.01));
}

static void millisRollover() {
  reset(0xFFFFFFF0UL);
  NHThermistor t(1);
  precise(t);
  pinRatio(1, 0.5);
  t.begin();
  pinRatio(1, ratio(betaOhms(30)));
  at(0x50);  // 96 ms later, after millis() wrapped
  CHECK(!t.update());
  at(0x54);  // 100 ms later
  CHECK(t.update());
  CHECK(NEAR(t.celsius(), 30, 0.02));
}

// ---- Board defaults --------------------------------------------------------

#if defined(ARDUINO_ARCH_ESP32)
static void esp32Defaults() {
  reset();
  mv[1] = 1650;  // half of the 3.3 V supply
  NHThermistor t(1);
  t.begin();
  CHECK(NEAR(t.celsius(), 25.0, 0.01));
}

static void esp32GroundedPinStillReadsAFewHundredMillivolts() {
  // A grounded pin reads about 142 mV through ESP32's calibration.
  reset();
  mv[1] = 142;
  NHThermistor t(1);
  t.begin();
  CHECK(t.status() == NH_NTC_SHORT);
  t.setWiring(NH_NTC_TO_VCC);
  CHECK(t.status() == NH_NTC_OPEN);
}

static void esp32TopOfRangeIsAFault() {
  reset();
  mv[1] = 3100;  // pulled up to 3.3 V, which the ADC flattens to about 3.1 V
  NHThermistor t(1);
  t.begin();
  CHECK(t.status() == NH_NTC_OPEN);
  t.setWiring(NH_NTC_TO_VCC);
  CHECK(t.status() == NH_NTC_SHORT);
  mv[1] = 2800;  // still below the 2.9 V limit
  t.begin();
  CHECK(t.isOk());
}

static void esp32SetReferenceUsesAnalogRead() {
  reset();
  adc[1] = 2048;  // half of 12-bit
  NHThermistor t(1);
  t.setReference(3.3f, 12);
  t.begin();
  CHECK(NEAR(t.celsius(), 25.0, 0.01));
}
#endif

#if defined(ARDUINO_ARCH_ESP8266)
// NodeMCU: 220k + 100k from A0 to GND, and a 3.2 V full scale. `rail` is the real 3.3 V rail.
static int nodeMcuCount(double ntcOhms, bool toGnd = true, double rail = 3.3) {
  const double load = 320000, series = 10000;
  double top = toGnd ? series : ntcOhms;
  double bottom = toGnd ? ntcOhms : series;
  bottom = bottom * load / (bottom + load);
  double pin = rail * bottom / (top + bottom);
  int count = (int)lround(pin / 3.2 * 1024);
  return count > 1023 ? 1023 : count;
}

static void nodeMcuLoadIsTakenOut() {
  const double temps[] = {0, 25, 60};
  for (double c : temps) {
    reset();
    NHThermistor t(1);
    adc[1] = nodeMcuCount(betaOhms(c));
    t.begin();
    CHECK(NEAR(t.celsius(), c, 0.15));  // 10-bit steps
    t.setWiring(NH_NTC_TO_VCC);
    adc[1] = nodeMcuCount(betaOhms(c), false);
    t.begin();
    CHECK(NEAR(t.celsius(), c, 0.15));
  }
}

static void nodeMcuOpenEvenWithALowRail() {
  const double rails[] = {3.3, 3.25, 3.2};
  for (double rail : rails) {
    reset();
    NHThermistor t(1);
    adc[1] = nodeMcuCount(1e12, true, rail);
    t.begin();
    CHECK(t.status() == NH_NTC_OPEN);
  }
}

static void setReferenceDropsTheBoardDivider() {
  reset();
  adc[1] = 512;
  NHThermistor t(1);
  t.setReference(3.3f, 10);  // your own ADC setup: ratiometric, no load
  t.begin();
  CHECK(NEAR(t.celsius(), 25.0, 0.01));
}
#endif

int main() {
#if defined(ARDUINO_ARCH_ESP32)
  esp32Defaults();
  esp32GroundedPinStillReadsAFewHundredMillivolts();
  esp32TopOfRangeIsAFault();
  esp32SetReferenceUsesAnalogRead();
#elif defined(ARDUINO_ARCH_ESP8266)
  nodeMcuLoadIsTakenOut();
  nodeMcuOpenEvenWithALowRail();
  setReferenceDropsTheBoardDivider();
#else
  defaultReads25AtHalfScale();
#endif
  betaAcrossTheRange();
  hundredKThermistor();
  wiringToVcc();
  openAndShortWithNtcToGnd();
  openAndShortWithNtcToVcc();
  railThresholds();
  separateSupply();
  steinhartHart();
  threePointsFromDatasheet();
  badCurvesAreRejected();
  invalidSettingsAreIgnored();
  offsetAndFahrenheit();
  readsOnlyEvery100ms();
  filterTimeConstant();
  settingsAfterBeginApplyAtOnce();
  switchingPinsDoesNotSpoilReadings();
  worksWithoutBegin();
  millisRollover();

#if defined(ARDUINO_ARCH_ESP32)
  printf("ESP32 defaults: ");
#elif defined(ARDUINO_ARCH_ESP8266)
  printf("ESP8266 defaults: ");
#endif
  printf("%d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}
