/* ============================================================
   Smart Helmet Safety System - Wokwi simulation (Arduino Uno)
   Shri's C-language course project - mirrors smart-helmet/src

   Mapping from the desktop C project:
     C module            ->  here
     sensors.c  MQ-3     ->  wokwi-gas-sensor (MQ-2) on A0
     sensors.c  MPU-6050 ->  wokwi-mpu6050 on I2C (crash = |a| > 4 g)
     sensors.c  strap    ->  pushbutton on pin 5 (pressed = worn)
     helmet.c   verdicts ->  interlock() below
     alert.c    SMS      ->  Serial Monitor "SMS" messages
     alert.c    buzzer   ->  buzzer on pin 2 + LEDs: 3 = GO, 4 = LOCK
   ============================================================ */

#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <LiquidCrystal_I2C.h>

/* ------------------- pins & thresholds ------------------- */
const int PIN_GAS     = A0;   // MQ-2 analog out (AO)
const int PIN_BUZZER  = 2;
const int PIN_LED_GO  = 3;    // green: ignition allowed
const int PIN_LED_LOCK= 4;    // red:   ignition locked
const int PIN_STRAP   = 5;    // helmet strap button (INPUT_PULLUP)

const float ALCOHOL_LIMIT_G = 1.00;  // reading over this = drunk
const float GAS_BASELINE_V  = 2.50;  // MQ-2 clean-air output (~400 ppm)
                             // measured live: sober air reads ~1.6-3.4 V
#define DEMO_AUTO_STRAP 0    // 1 = auto "strap on" for hands-free demo
const float CRASH_THRESHOLD = 4.00;  // |accel| in g over this = crash

/* ------------------- state machine ------------------------- */
enum HelmetState { STATE_PRECHECK, STATE_RIDING, STATE_CRASH, STATE_BLOCKED };
HelmetState state = STATE_PRECHECK;

Adafruit_MPU6050 mpu;
LiquidCrystal_I2C lcd(0x27, 16, 2);

unsigned long tickStart = 0;
const unsigned long TICK_MS = 1000;  // one sensor tick per second
int tickNo = 0;
float maxAlcohol = 0, maxImpact = 0;

/* --------- optional: auto-demo crash at tick 8 --------- */
/* Wokwi has no "shake the sensor" mouse action, so a crash is
   simulated automatically once per ride (tick 8). You can also
   drag the MPU6050 accel sliders live while it runs.        */
const int AUTO_CRASH_TICK = 8;
const int AUTO_GAS_TICK   = 11;      // 0 = never (set > ride length)

/* ============================================================ */
void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED_GO,   OUTPUT);
  pinMode(PIN_LED_LOCK, OUTPUT);
  pinMode(PIN_STRAP,    INPUT_PULLUP);
  pinMode(PIN_BUZZER,   OUTPUT);

  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.print("SMART HELMET");
  lcd.setCursor(0, 1);
  lcd.print("v1.0  booting...");

  Serial.println("=========================================");
  Serial.println("  SMART HELMET SAFETY SYSTEM (Wokwi)");
  Serial.println("=========================================");
  Serial.println("SMS target: +91-9876543210 (simulated)");
  Serial.println();

  if (!mpu.begin()) {
    Serial.println("MPU6050 not found! Check wiring (SDA=A4, SCL=A5).");
    lcd.clear();
    lcd.print("MPU6050 ERROR");
    while (true) { delay(100); }
  }
  Serial.println("MPU6050 ready. Pre-ride check every 2 s...");
}

/* ------------------- helpers ------------------- */
float readAlcohol() {
  /* MQ-2 AO gives 0..1023; convert to volts, subtract the clean-air
     baseline and report only the excess - same "ppm above baseline"
     semantics as the C project. Clean air ~ 0; slide the ppm slider
     up (5000+) to simulate a drunk rider (~2.5 = well over limit). */
  int raw = analogRead(PIN_GAS);
  float volts = raw * 5.0 / 1023.0;
  float above = volts - GAS_BASELINE_V;
  return (above < 0) ? 0 : above;
}

float readImpactG() {
  /* Magnitude of acceleration minus 1 g resting gravity. */
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  float x = a.acceleration.x / 9.80665;
  float y = a.acceleration.y / 9.80665;
  float z = a.acceleration.z / 9.80665;
  float mag = sqrtf(x * x + y * y + z * z);
  float dynamic = mag - 1.0;
  return (dynamic < 0) ? 0 : dynamic;
}

bool helmetWorn() {
#if DEMO_AUTO_STRAP
  /* Hands-free demo: strap counts as closed for the first 25 s. */
  if (millis() < 25000UL) return true;
#endif
  return digitalRead(PIN_STRAP) == LOW;   // pressed = strap closed
}

void setLeds(bool go) {
  digitalWrite(PIN_LED_GO,   go ? HIGH : LOW);
  digitalWrite(PIN_LED_LOCK, go ? LOW  : HIGH);
}

void beep(int times, int hz) {
  for (int i = 0; i < times; i++) {
    tone(PIN_BUZZER, hz, 150);
    delay(180);
  }
  noTone(PIN_BUZZER);
}

void sendSMS(const char *type, const char *msg) {
  Serial.println("+----------------------------------------+");
  Serial.println("|        SMS ALERT TRANSMITTED           |");
  Serial.println("+----------------------------------------+");
  Serial.print("  To      : +91-9876543210\n");
  Serial.print("  Type    : "); Serial.println(type);
  Serial.print("  Message : "); Serial.println(msg);
  Serial.println("+----------------------------------------+");
}

void showBlocked(const char *why) {
  state = STATE_BLOCKED;
  setLeds(false);
  beep(3, 880);
  lcd.clear();
  lcd.print("IGNITION LOCKED");
  lcd.setCursor(0, 1);
  lcd.print(why);
  sendSMS("INTERLOCK", why);
}

void showCrash() {
  state = STATE_CRASH;
  setLeds(false);
  beep(5, 2000);
  lcd.clear();
  lcd.print("CRASH DETECTED!");
  lcd.setCursor(0, 1);
  lcd.print("SMS + buzzer ON");
  sendSMS("CRASH ALERT", "Crash detected! Impact over 4g. Send help.");
}

/* ------------------- main loop ------------------- */
void loop() {
  /* -------- PRE-RIDE INTERLOCK CHECK (every 2 s) -------- */
  if (state == STATE_PRECHECK) {
    float alc = readAlcohol();
    bool worn = helmetWorn();
    static unsigned long last = 0;
    if (millis() - last < 2000) return;
    last = millis();

    Serial.print("[PRE-RIDE] helmet="); Serial.print(worn ? "ON " : "OFF");
    Serial.print(" alcohol=");         Serial.print(alc, 2);
    Serial.print(" (limit ");          Serial.print(ALCOHOL_LIMIT_G, 2);
    Serial.println(")");

    lcd.clear();
    if (!worn) {
      lcd.print("PUT ON HELMET");
      lcd.setCursor(0, 1);
      lcd.print("strap switch off");
      setLeds(false);
    } else if (alc > ALCOHOL_LIMIT_G) {
      lcd.print("ALCOHOL HIGH!");
      lcd.setCursor(0, 1);
      lcd.print("ignition locked");
      showBlocked("Helmet OK but alcohol over limit - ride blocked.");
    } else {
      state = STATE_RIDING;
      tickNo = 0;
      maxAlcohol = maxImpact = 0;
      setLeds(true);
      lcd.print("RIDE STARTED");
      lcd.setCursor(0, 1);
      lcd.print("ride safe!");
      Serial.println(">> Ignition ON. Ride started.");
      beep(1, 1200);
      delay(600);
    }
    return;
  }

  /* -------- RIDING: one sensor tick per second -------- */
  if (state == STATE_RIDING) {
    if (millis() - tickStart < TICK_MS) return;
    tickStart = millis();
    tickNo++;

    /* auto-demo anomalies (also test live with the sliders) */
    if (tickNo == AUTO_CRASH_TICK) {
      /* emulate a sudden 6 g impact on all axes */
      Serial.println("<< SIMULATED IMPACT (6 g) >>");
      maxImpact = 6.0;
      showCrash();
      return;
    }
    if (AUTO_GAS_TICK > 0 && tickNo == AUTO_GAS_TICK) {
      Serial.println("<< SIMULATED ALCOHOL SPIKE >>");
    }

    float alc = readAlcohol();
    float imp = readImpactG();
    if (alc > maxAlcohol) maxAlcohol = alc;
    if (imp > maxImpact)  maxImpact  = imp;

    Serial.print("Tick "); Serial.print(tickNo);
    Serial.print(" | alcohol "); Serial.print(alc, 2);
    Serial.print(" | impact ");  Serial.print(imp, 2);
    Serial.print(" g | helmet "); Serial.println(helmetWorn() ? "ON" : "OFF");

    lcd.clear();
    lcd.print("A:"); lcd.print(alc, 1);
    lcd.print(" G:"); lcd.print(imp, 1);
    lcd.setCursor(0, 1);
    lcd.print("tick "); lcd.print(tickNo);

    if (imp >= CRASH_THRESHOLD) { showCrash(); return; }
    if (!helmetWorn()) {
      lcd.clear(); lcd.print("HELMET REMOVED!");
      showBlocked("Helmet removed while riding - ride ended.");
      return;
    }
    if (alc > ALCOHOL_LIMIT_G) {
      showBlocked("Alcohol over limit while riding - ride ended.");
      return;
    }
    return;
  }

  /* -------- terminal states: hold, explain, re-arm -------- */
  if (state == STATE_CRASH || state == STATE_BLOCKED) {
    static unsigned long lockSince = 0;
    if (lockSince == 0) lockSince = millis();

    /* A BLOCKED ride re-arms automatically once the sensors are safe
       again; a CRASH stays locked until RESET (safety-critical). */
    if (state == STATE_BLOCKED && millis() - lockSince > 3000) {
      if (helmetWorn() && readAlcohol() <= ALCOHOL_LIMIT_G) {
        lockSince = 0;
        state = STATE_PRECHECK;
        Serial.println(">> Sensors normal again - system re-armed.");
        lcd.clear();
        lcd.print("SYSTEM RE-ARMED");
        delay(800);
        return;
      }
    }

    Serial.println(state == STATE_CRASH
      ? "(crash lock - press RST for a new ride)"
      : "(locked - fix the sensors to re-arm, or press RST)");
    delay(3000);
  }
}
