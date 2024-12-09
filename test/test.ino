#include <Arduino.h>

#define LED_GREEN_PIN 17

void setup() {
  Serial.begin(115200);

  pinMode(LED_GREEN_PIN, OUTPUT);

  ledcAttach(LED_GREEN_PIN, 5000, 8);
}

void loop() {

  ledcWrite(LED_GREEN_PIN, 255); //255 -> off
  delay(3000);

  ledcWrite(LED_GREEN_PIN, 0); //0 -> on
  delay(10000);
}