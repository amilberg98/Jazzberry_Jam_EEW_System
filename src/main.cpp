#include <Arduino.h>
#include "accelADC.h"
#include "esp_adc_cal.h"

#define BAT_ADC_PIN 35 // using GPIO 35 for battery since built-in voltage divider
accelADC accel(36, 39, 34); // NOTE the change to pin 34

void setup() {
  Serial.begin(9600);
  accel.begin();
}

// readBatteryVoltage
// Gets current battery voltage using GPIO35 built-in voltage divider
float readBatteryVoltage() {
  uint32_t raw = analogRead(BAT_ADC_PIN);
  return (raw / 1000.0);
}


void loop() {
  if (!accel.sampleReady()) return;

  AccelData sample = accel.read();

  Serial.print(sample.ax, 6); Serial.print(", ");
  Serial.print(sample.ay, 6); Serial.print(", ");
  Serial.println(sample.az, 6);


  Serial.printf("Battery: %.2f V\n", readBatteryVoltage());
  
  
  delay(5000);
}
