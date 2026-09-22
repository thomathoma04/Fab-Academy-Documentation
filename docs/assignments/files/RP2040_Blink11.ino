#include <Adafruit_NeoPixel.h>

#define POWER_PIN 11
#define DATA_PIN  12
#define NUMPIXELS 1

// Initialize the NeoPixel library
Adafruit_NeoPixel pixels(NUMPIXELS, DATA_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  pinMode(POWER_PIN, OUTPUT);
  digitalWrite(POWER_PIN, HIGH); // Enable RGB LED power
  pixels.begin();
}

void loop() {
  
  
  pixels.setPixelColor(0, pixels.Color(255, 255, 0)); // Set color to Red
  pixels.show(); // Push color to LED
  delay(500);
  pixels.setPixelColor(0, pixels.Color(0, 255, 255)); // Set color to Red
  pixels.show(); // Push color to LED
  delay(500);
  pixels.setPixelColor(0, pixels.Color(255, 0, 255)); // Set color to Red
  pixels.show(); // Push color to LED
  delay(500);
  digitalWrite(POWER_PIN, LOW); // Disable RGB LED power
  delay(500);
}