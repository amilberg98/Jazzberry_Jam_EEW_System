#include <Arduino.h>

const int xPin = A0;
const int yPin = A1;
const int zPin = A2;

void setup() {
  Serial.begin(9600);
}

void loop() {
  float xRaw = analogRead(xPin);
  float yRaw = analogRead(yPin);
  float zRaw = analogRead(zPin);

  Serial.print("X: ");
  Serial.print(xRaw);
  Serial.print("\tY: ");
  Serial.print(yRaw);
  Serial.print("\tZ: ");
  Serial.println(zRaw);

  delay(500);
}

