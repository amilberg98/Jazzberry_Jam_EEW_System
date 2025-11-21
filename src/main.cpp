#include <Arduino.h>


/* Constants
 * xPin/yPin/zPin: For X, Y, Z readings from accelerometer
 * SHAKE_THRESHOLD: Difference from previous shake that we consider an earthquake
 * lastX/lastY/lastZ: Previous X, Y, Z readings
 * alertPin: Pin for alerts to be sent out on
*/
const int xPin = A0;
const int yPin = A1;
const int zPin = A2;
const int SHAKE_THRESHOLD = 235;
int lastX = 0;
int lastY = 0;
int lastZ = 0;
int alertPin = 11;

void setup() {
  Serial.begin(9600);

  lastX = analogRead(xPin);
  lastY = analogRead(yPin);
  lastZ = analogRead(zPin);
  pinMode(alertPin, OUTPUT);
}

void loop() {
  int xRaw = analogRead(xPin);
  int yRaw = analogRead(yPin);
  int zRaw = analogRead(zPin);

  // Compute horizontal motion, combine difference readings
  int dx = abs(xRaw - lastX);
  int dy = abs(yRaw - lastY);
  int dz = abs(zRaw - lastZ);
  int shakeMagnitude = dx + dy; //+ dz;

  //UNCOMMENT TO PRINT DEBUG VALUES
  // Serial.print("X: ");
  // Serial.print(xRaw);
  // Serial.print("\tY: ");
  // Serial.print(yRaw);
  // Serial.print("\tZ: ");
  // Serial.println(zRaw);
  //Serial.print("\tShake: ");
  //Serial.println(shakeMagnitude);


  // Alert if we shake too hard
  if (shakeMagnitude > SHAKE_THRESHOLD) {
    digitalWrite(alertPin, HIGH); 
  } else {
    digitalWrite(alertPin, LOW); 
  }

  // Save values and delay slightly beofre next loop
  lastX = xRaw;
  lastY = yRaw;
  lastZ = zRaw;
  delay(100);
}

