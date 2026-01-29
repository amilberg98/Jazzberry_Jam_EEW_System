#include <Arduino.h>
#include "accelADC.h"

accelADC accel(32, 33, 34);

void setup() {
  Serial.begin(115200);
  accel.begin();
}

void loop() {
  if (!accel.sampleReady()) return;

  AccelData sample = accel.read();

  Serial.print(sample.ax, 6); Serial.print(", ");
  Serial.print(sample.ay, 6); Serial.print(", ");
  Serial.println(sample.az, 6);

}
