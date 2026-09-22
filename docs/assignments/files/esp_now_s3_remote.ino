// XIAO ESP32-S3: Remote Alarm Stop Control
// - Receives alarm signal from C6 → LED flashes
// - MX switch press → sends stop signal to C6 → stops alarm

#include <esp_now.h>
#include <WiFi.h>

// ── MAC Address of XIAO ESP32-C6 (main controller) ──
uint8_t peerMacAddress[] = {0x10, 0x51, 0xDB, 0x1A, 0x99, 0x10};  // C6: 10:51:DB:1A:99:10

// ── Pins ────────────────────────────────────────────────
#define PIN_BUTTON      3   // Cherry MX switch on D2 (GPIO3, active low with pullup)
#define PIN_LED         1   // External LED on D0 (GPIO1)
#define PIN_ONBOARD_LED 21  // Onboard LED (GPIO21)

// ── Message structure ────────────────────────────────────
typedef struct {
  char type;  // 'A' = alarm start, 'S' = stop alarm
} Message;

// ── Button state ─────────────────────────────────────────
int lastButtonState = HIGH;
unsigned long lastButtonTime = 0;
unsigned long lastStopSignal = 0;
const unsigned long DEBOUNCE_TIME = 50;
const unsigned long STOP_COOLDOWN = 500;  // Prevent spam

// ── Alarm state ──────────────────────────────────────────
bool alarmActive = false;
unsigned long lastLedFlash = 0;
const unsigned long LED_FLASH_INTERVAL = 150;  // match C6's flash rate

void setup() {
  Serial.begin(115200);

  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_ONBOARD_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  digitalWrite(PIN_ONBOARD_LED, LOW);

  lastButtonState = digitalRead(PIN_BUTTON);

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

  // Add peer (C6)
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, peerMacAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    while(1);
  }

  // Print S3's own MAC
  Serial.print("S3 MAC: ");
  Serial.println(WiFi.macAddress());

  Serial.println("=== ESP-NOW Alarm Remote ===");
  Serial.println("Waiting for alarm from C6...");
  Serial.println("Press MX switch to stop alarm");
}

void loop() {
  // ── Read Cherry MX button ────────────────────────────
  int buttonState = digitalRead(PIN_BUTTON);

  // Simple button press detection: LOW state + alarm active + cooldown passed
  if (buttonState == LOW && alarmActive && millis() - lastStopSignal >= STOP_COOLDOWN) {
    Serial.println("✓ Stop button pressed! Sending stop signal to C6...");

    Message msg;
    msg.type = 'S';
    esp_now_send(peerMacAddress, (uint8_t *)&msg, sizeof(msg));

    alarmActive = false;
    digitalWrite(PIN_LED, LOW);
    digitalWrite(PIN_ONBOARD_LED, LOW);
    lastStopSignal = millis();  // Prevent spam
  }

  lastButtonState = buttonState;

  // ── LED flash during alarm ──────────────────────────
  if (alarmActive) {
    unsigned long now = millis();

    if (now - lastLedFlash >= LED_FLASH_INTERVAL) {
      static bool ledState = false;
      ledState = !ledState;
      digitalWrite(PIN_LED, ledState ? HIGH : LOW);
      digitalWrite(PIN_ONBOARD_LED, ledState ? HIGH : LOW);  // Flash onboard LED in sync
      lastLedFlash = now;
    }
  } else {
    // LED off when alarm not active
    digitalWrite(PIN_ONBOARD_LED, LOW);
  }
}

void onDataRecv(const esp_now_recv_info *info, const uint8_t *data, int len) {
  if (len != sizeof(Message)) {
    return;
  }

  Message msg;
  memcpy(&msg, data, sizeof(msg));

  if (msg.type == 'A') {
    // Alarm signal from C6
    Serial.println("Alarm received from C6! Press MX switch to stop...");
    alarmActive = true;
    lastLedFlash = millis();
  }
}
