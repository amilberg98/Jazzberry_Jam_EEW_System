#include <Arduino.h>
#include "accelADC.h"
#include "radio.h"
#include "biquad.h"
#include "EEWStateMachine.h"
#include "STALTA.h"

accelADC accel(36, 39, 35);
Radio radio(18, 26, 23);
Biquad filter;
EEWStateMachine eews(accel, filter, radio);

void setup() {
  Serial.begin(9600);
  filter.reset();
  radio.begin(433.0);
  eews.begin();
}

void loop() {
  eews.update();
}
