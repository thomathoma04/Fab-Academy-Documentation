#include <Adafruit_NeoPixel.h>

// ── Pins ────────────────────────────────────────────────
#define PIN_LED      22
#define PIN_BUZZ     17
#define PIN_A        1    // EC11 CLK
#define PIN_B        2    // EC11 DT
#define PIN_NEOPIXEL 18
#define NUM_LEDS     12

Adafruit_NeoPixel ring(NUM_LEDS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// ── State machine ───────────────────────────────────────
enum State { IDLE, COUNTING, RUNNING, ALARMING };
State state = IDLE;

// ── Encoder ─────────────────────────────────────────────
int lastA           = 0;
int count           = 0;
int pending         = 0;
unsigned long lastPulse = 0;
const unsigned long GAP = 1000;

// ── NeoPixel countdown ──────────────────────────────────
int lit             = 0;
unsigned long lastDim = 0;
const unsigned long DIM_INTERVAL = 2000;  // 10 seconds

// ── Buzzer alarm ────────────────────────────────────────
unsigned long alarmStart = 0;
const unsigned long TONE_DURATION = 1000;  // buzzer plays for 1 second

// ── Helper: show N LEDs lit ─────────────────────────────
void showRing(int n) {
  ring.clear();
  for (int i = 0; i < n; i++) {
    ring.setPixelColor(i, ring.Color(0, 210, 160));  // teal
  }
  ring.show();
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_LED,  OUTPUT);
  pinMode(PIN_BUZZ, OUTPUT);
  pinMode(PIN_A,    INPUT_PULLUP);
  pinMode(PIN_B,    INPUT_PULLUP);

  ring.begin();
  ring.setBrightness(80);
  ring.clear();
  ring.show();

  lastA = digitalRead(PIN_A);

  Serial.println("=== NeoPixel Countdown Timer Test ===");
  Serial.println("1. Rotate encoder to set number of turns (1-12)");
  Serial.println("2. Stop rotating — countdown will start automatically");
  Serial.println("3. One LED dims every 10 seconds");
  Serial.println("4. When all LEDs are dimmed, buzzer sounds for 1 second");
}

void loop() {

  // ── Button / Encoder reading (if in IDLE or COUNTING state) ──
  if (state == IDLE || state == COUNTING) {
    int currentA = digitalRead(PIN_A);

    if (currentA != lastA) {
      int dir = (digitalRead(PIN_B) != currentA) ? 1 : -1;
      if (dir == 1) {
        pending   = 1;
        lastPulse = millis();
      }
    }
    lastA = currentA;

    // Commit the turn after GAP period
    if (pending != 0 && millis() - lastPulse >= GAP) {
      count += pending;
      count = constrain(count, 0, NUM_LEDS);  // clamp to 0-12
      pending = 0;

      // Update display
      showRing(count);
      digitalWrite(PIN_LED, HIGH);
      delay(100);
      digitalWrite(PIN_LED, LOW);

      Serial.print("Turns set: ");
      Serial.println(count);

      state = COUNTING;
    }
  }

  // ── If in COUNTING and no new input for 3 seconds, start countdown ──
  if (state == COUNTING && millis() - lastPulse >= 2000) {
    if (count > 0) {
      lit       = count;
      state     = RUNNING;
      lastDim   = millis();
      Serial.print("Starting countdown with ");
      Serial.print(lit);
      Serial.println(" LEDs");
    }
  }

  // ── Countdown timer (RUNNING state) ──────────────────────────
  if (state == RUNNING && millis() - lastDim >= DIM_INTERVAL) {
    lastDim = millis();
    lit--;

    showRing(lit);

    if (lit > 0) {
      Serial.print("LEDs remaining: ");
      Serial.println(lit);
    } else {
      Serial.println("All LEDs dimmed — ALARM!");
      state      = ALARMING;
      alarmStart = millis();
      ring.clear();
      ring.show();
      tone(PIN_BUZZ, 2730, TONE_DURATION);  // play for 1 second, then stop automatically
    }
  }

  // ── Alarm: wait for buzzer duration, then reset ──────────────
  if (state == ALARMING && millis() - alarmStart >= TONE_DURATION) {
    // Flash all LEDs once
    for (int i = 0; i < NUM_LEDS; i++) {
      ring.setPixelColor(i, ring.Color(0, 210, 160));  // teal
    }
    ring.show();
    delay(200);
    ring.clear();
    ring.show();

    state = IDLE;
    count = 0;
    Serial.println("\nAlarm finished — back to IDLE");
    Serial.println("Rotate encoder to start a new countdown");
  }
}
