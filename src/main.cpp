#include <Arduino.h>
#include "accelADC.h"
#include "radio.h"
#include "biquad.h"
#include "EEWStateMachine.h"
#include "STALTA.h"
#include "taucpd.h"

accelADC accel(36, 39, 35);
Radio radio(18, 26, 23);
Biquad filter;
STALTA stalta;
TauCPdEstimator taucpd(accel.SAMPLE_RATE_HZ, 3.0f); // TODO maybe modify sample rate
EEWStateMachine eews(accel, filter, radio, stalta, taucpd);

void setup() {
  Serial.begin(9600);
  accel.begin();
  filter.reset();
  radio.begin(433.0);
  taucpd.reset();
  eews.begin();
}

void loop() {
  eews.update();
}
