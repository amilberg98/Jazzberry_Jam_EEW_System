#include <Arduino.h>
#include "accelADC.h"

accelADC accel(36, 39, 35);

void setup() {
  Serial.begin(9600);
  accel.begin();
}

void loop() {
  if (!accel.sampleReady()) return;

  AccelData sample = accel.read();

  Serial.print(sample.ax, 6); Serial.print(", ");
  Serial.print(sample.ay, 6); Serial.print(", ");
  Serial.println(sample.az, 6);
  delay(300);
}
