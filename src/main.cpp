#include <Arduino.h>
#include "accelADC.h"
#include "radio.h"
#include "biquad.h"
#include "EEWStateMachine.h"
#include "STALTA.h"
#include "taucpd.h"
#include "Alert.h"

// Pin mappings and constants
const int X_PIN = 36;
const int Y_PIN = 39;
const int Z_PIN = 35;
const int LORA_CS_PIN = 18;
const int LORA_IRQ_PIN = 26;
const int LORA_RST_PIN = 23;
const float TAUCPD_SAMPLE_WINDOW = 3.0f;
const int BUZZER_PIN = 13;

// Class constructors
accelADC accel(X_PIN, Y_PIN, Z_PIN);
Radio radio(LORA_CS_PIN, LORA_IRQ_PIN, LORA_RST_PIN);
Biquad filterX, filterY, filterZ;
STALTA stalta;
TauCPdEstimator taucpd(accel.SAMPLE_RATE_HZ, TAUCPD_SAMPLE_WINDOW); // TODO maybe modify sample rate
Alert alert(radio, BUZZER_PIN, taucpd); // Buzzer on pin 13
EEWStateMachine eews(accel, filter, radio, stalta, taucpd, alert);

// Initializing each portion of EEWS
void setup() {
  Serial.begin(9600);
  accel.begin();

  filterX.reset();
  filterY.reset();
  filterZ.reset();

  radio.begin(433.0);
  taucpd.reset();
  eews.begin();
}

// Running main EEWS state machine
void loop() {
  eews.update();
}
