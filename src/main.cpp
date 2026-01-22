#include <Arduino.h>
#include "esp_adc_cal.h"

/* ----- USER CONFIG VARIABLES -- TODO: May need to update with more testing/research ----- */
// ADC pins
const int xPin = 32;   
const int yPin = 33;   
const int zPin = 34; 

// Sampling Rate
// 200Hz to detect up to 100Hz based on Nyquist, should cover all earthquake-related vibrations
const float SAMPLE_RATE_HZ = 200.0;
const uint32_t SAMPLE_PERIOD_US = 1000000 / SAMPLE_RATE_HZ;

// Bias Sampling Accuracy
const int BIAS_SAMPLES = 1000;

// ADXL354 sensitivity
const float ADXL_SENSITIVITY_MV_PER_G = 100.0; // Based on ANALOG OUTPUT FOR THE ADXL354 on the Datasheet at +-8g
const float G_TO_ACCEL = 9.80665;

/* ----------------------------------------- */

esp_adc_cal_characteristics_t adc_chars;

uint32_t bias_x_mv = 0;
uint32_t bias_y_mv = 0;
uint32_t bias_z_mv = 0;

uint32_t last_sample_time = 0;

// calibrate_bias
// Finds ADC's bias using esp_adc_cal.h library. TODO: May need to add library
void calibrate_bias() {
  uint64_t sum_x = 0, sum_y = 0, sum_z = 0;

  for (int i = 0; i < BIAS_SAMPLES; i++) {
    sum_x += esp_adc_cal_raw_to_voltage(analogRead(xPin), &adc_chars);
    sum_y += esp_adc_cal_raw_to_voltage(analogRead(yPin), &adc_chars);
    sum_z += esp_adc_cal_raw_to_voltage(analogRead(zPin), &adc_chars);
    delay(2);
  }

  bias_x_mv = sum_x / BIAS_SAMPLES;
  bias_y_mv = sum_y / BIAS_SAMPLES;
  bias_z_mv = sum_z / BIAS_SAMPLES;

  Serial.println("Bias calibration complete");
}

void setup() {
  Serial.begin(115200);

  // ADC Config
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // ADC Calibration 
  // NOTE: Assumes we use ADC Unit 1, 11dB attenuation (to read 0-3.3V), 12-bit ADC resolution, 1.1V Vref
  // Above values are consistent with LilyGO T-Beam
  esp_adc_cal_characterize(
    ADC_UNIT_1,
    ADC_ATTEN_DB_11,
    ADC_WIDTH_BIT_12,
    1100,
    &adc_chars
  );

  calibrate_bias();
}

void loop() {

  // Ensure consistent sampling rate
  uint32_t now = micros();
  if (now - last_sample_time < SAMPLE_PERIOD_US) return;
  last_sample_time = now;

  // Read ADC values (voltage) considering nonlinear parameters
  uint32_t x_mv = esp_adc_cal_raw_to_voltage(analogRead(xPin), &adc_chars);
  uint32_t y_mv = esp_adc_cal_raw_to_voltage(analogRead(yPin), &adc_chars);
  uint32_t z_mv = esp_adc_cal_raw_to_voltage(analogRead(zPin), &adc_chars);

  // Bias removal
  float x_centered_mv = (float)x_mv - bias_x_mv;
  float y_centered_mv = (float)y_mv - bias_y_mv;
  float z_centered_mv = (float)z_mv - bias_z_mv;

  // Convert to acceleration
  float ax = (x_centered_mv / ADXL_SENSITIVITY_MV_PER_G) * G_TO_ACCEL;
  float ay = (y_centered_mv / ADXL_SENSITIVITY_MV_PER_G) * G_TO_ACCEL;
  float az = (z_centered_mv / ADXL_SENSITIVITY_MV_PER_G) * G_TO_ACCEL;

  // Debug output
  Serial.print(ax, 6); Serial.print(", ");
  Serial.print(ay, 6); Serial.print(", ");
  Serial.println(az, 6);
}
