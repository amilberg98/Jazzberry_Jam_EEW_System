#include <Arduino.h>
#include "accelADC.h"
#include "radio.h"
#include "biquad.h"
#include "STALTA.h"

accelADC accel(36, 39, 35);
Radio radio(18, 26, 23);

void setup() {
  Serial.begin(9600);
  accel.begin();

  if (radio.begin(433.0) != RADIOLIB_ERR_NONE) {
    Serial.println("Radio init failed!");
    while (1);
  }

  radio.startListen();
}

void loop() {
  if (!accel.sampleReady()) return;

  if (radio.msgAvailable()) {
    String msg = radio.recv();
    Serial.println("Received: " + msg);
  }

  uint8_t data[] = "ALERT";
  int result = radio.send(data, sizeof(data));
  if (result == RADIOLIB_ERR_NONE) {
    Serial.println("Transmitted Successfully");
  } else {
    Serial.println("Transmission failed");
  }
  AccelData sample = accel.read();
 
  Biquad filter;
  StaLtaDetector detector;

  float filtered_sample = filter.process(sample.ax);
  bool triggered = detector.update(filtered_sample);

  

  // Serial.print(sample.ax, 6); Serial.print(", ");
  // Serial.print(sample.ay, 6); Serial.print(", ");
  // Serial.println(sample.az, 6);
  delay(5000);
}
