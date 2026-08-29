
void setup() {
  pinMode(D7, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(D6, INPUT_PULLUP);
  //Serial.begin(115200); // Initialize serial communication at 115200 baud
}

void loop() {
  int ButtonValue = digitalRead(D6);
  digitalWrite(D7, HIGH);  // turn the LED on (HIGH is the voltage level)

 if (ButtonValue == LOW) {
    digitalWrite(D7, LOW);
    Serial.print("Button Value: ");
    Serial.println(ButtonValue); // Print the value followed by a newline
    delay(4000);
  } 
  else {
    digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
  }  

}
