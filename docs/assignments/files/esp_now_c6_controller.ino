// XIAO ESP32-C6: Countdown Timer Controller with ESP-NOW
// State machine: IDLE → COUNTING → RUNNING → ALARMING → IDLE
// - EC11 encoder sets countdown duration
// - NeoPixel shows countdown progress
// - Buzzer and wireless alert triggered at alarm
// - Continues until S3 MX button is pressed

#include <esp_now.h>
#include <WiFi.h>
#include <Adafruit_NeoPixel.h>

// ── MAC Address of XIAO ESP32-S3 (remote) ──
uint8_t peerMacAddress[] = {0x30, 0x30, 0xF9, 0x34, 0x60, 0x2C};  // S3: 30:30:F9:34:60:2C

// ── Pins ────────────────────────────────────────────────
#define PIN_LED      22
#define PIN_BUZZ     17
#define PIN_A        1    // EC11 CLK
#define PIN_B        2    // EC11 DT
#define PIN_NEOPIXEL 18
#define NUM_LEDS     12

Adafruit_NeoPixel ring(NUM_LEDS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// ── Message structure ────────────────────────────────────
typedef struct {
  char type;  // 'A' = alarm start, 'S' = stop alarm
} Message;

// ── State machine ───────────────────────────────────────
enum State { IDLE, COUNTING, RUNNING, ALARMING };
State state = IDLE;

// ── Encoder ─────────────────────────────────────────────
int lastA           = 0;
int count           = 0;
int pending         = 0;
unsigned long lastPulse = 0;
const unsigned long GAP = 300;  // 300ms debounce between turns

// ── NeoPixel countdown ──────────────────────────────────
int lit             = 0;
unsigned long lastDim = 0;
const unsigned long DIM_INTERVAL = 2000;

// ── Buzzer alarm ────────────────────────────────────────
unsigned long alarmStart = 0;
unsigned long lastBuzz = 0;
const unsigned long BUZZ_DURATION = 1000;  // beep for 1 second
const unsigned long BUZZ_INTERVAL = 1500;  // beep every 1.5 seconds
bool buzzerActive = false;

// ── NeoPixel flash during alarm ─────────────────────────
unsigned long lastFlash = 0;
const unsigned long FLASH_INTERVAL = 150;  // flash every 150ms

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

  // Initialize WiFi (required for ESP-NOW)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    while(1);
  }

  esp_now_register_send_cb([](const wifi_tx_info_t *info, esp_now_send_status_t status) {
    // Send callback
  });

  esp_now_register_recv_cb([](const esp_now_recv_info *info, const uint8_t *data, int data_len) {
    onDataRecv(info, data, data_len);
  });

  // Add peer (S3)
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, peerMacAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    while(1);
  }

  // Print C6's own MAC
  Serial.print("C6 MAC: ");
  Serial.println(WiFi.macAddress());

  Serial.println("=== ESP-NOW Countdown Timer ===");
  Serial.println("1. Rotate encoder to set duration (1-12)");
  Serial.println("2. Countdown starts after 2 seconds idle");
  Serial.println("3. Press MX switch on S3 to stop alarm");
}

void loop() {

  // ── Encoder reading (IDLE or COUNTING) ──────────────────
  if (state == IDLE || state == COUNTING) {
    int currentA = digitalRead(PIN_A);

    if (currentA != lastA) {
      int dir = (digitalRead(PIN_B) != currentA) ? 1 : -1;
      pending = dir;
      lastPulse = millis();
    }
    lastA = currentA;

    if (pending != 0 && millis() - lastPulse >= GAP) {
      count += pending;
      count = constrain(count, 0, NUM_LEDS);
      pending = 0;

      showRing(count);
      digitalWrite(PIN_LED, HIGH);
      delay(100);
      digitalWrite(PIN_LED, LOW);

      Serial.print("Turns set: ");
      Serial.println(count);

      state = COUNTING;
    }
  }

  // ── Auto-start countdown after 2 seconds idle ──────────
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

  // ── Countdown timer (RUNNING) ────────────────────────
  if (state == RUNNING && millis() - lastDim >= DIM_INTERVAL) {
    lastDim = millis();
    lit--;

    showRing(lit);

    if (lit > 0) {
      Serial.print("LEDs remaining: ");
      Serial.println(lit);
    } else {
      // Alarm triggered!
      Serial.println("All LEDs dimmed — ALARM!");
      state = ALARMING;
      alarmStart = millis();
      lastBuzz = millis();
      lastFlash = millis();
      buzzerActive = false;

      // Send alarm signal to S3
      Message msg;
      msg.type = 'A';
      esp_now_send(peerMacAddress, (uint8_t *)&msg, sizeof(msg));
      Serial.println("Alarm sent to S3!");

      // Start first beep
      tone(PIN_BUZZ, 2730);
      buzzerActive = true;
    }
  }

  // ── Alarm: continuous buzzer & NeoPixel flash ──────────
  if (state == ALARMING) {
    unsigned long now = millis();

    // Buzzer: beep for BUZZ_DURATION, then silent for gap, repeat
    if (buzzerActive && (now - lastBuzz >= BUZZ_DURATION)) {
      noTone(PIN_BUZZ);
      buzzerActive = false;
      lastBuzz = now;
    } else if (!buzzerActive && (now - lastBuzz >= BUZZ_INTERVAL - BUZZ_DURATION)) {
      tone(PIN_BUZZ, 2730);
      buzzerActive = true;
      lastBuzz = now;
    }

    // NeoPixel: flash all LEDs
    if (now - lastFlash >= FLASH_INTERVAL) {
      static bool flashState = false;
      flashState = !flashState;

      if (flashState) {
        for (int i = 0; i < NUM_LEDS; i++) {
          ring.setPixelColor(i, ring.Color(0, 210, 160));  // teal
        }
      } else {
        ring.clear();
      }
      ring.show();
      lastFlash = now;
    }
  }
}

void onDataRecv(const esp_now_recv_info *info, const uint8_t *data, int len) {
  if (len != sizeof(Message)) {
    return;
  }

  Message msg;
  memcpy(&msg, data, sizeof(msg));

  if (msg.type == 'S' && state == ALARMING) {
    // Stop alarm signal received from S3
    Serial.println("Stop signal from S3 — resetting!");
    noTone(PIN_BUZZ);
    ring.clear();
    ring.show();

    state = IDLE;
    count = 0;
    Serial.println("Back to IDLE. Rotate encoder to start new countdown.");
  }
}
