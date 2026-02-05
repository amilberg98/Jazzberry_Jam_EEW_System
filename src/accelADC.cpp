#include "accelADC.h"
#include "esp_adc_cal.h"

esp_adc_cal_characteristics_t adc_chars;

accelADC::accelADC(int xPin, int yPin, int zPin):
_xPin(xPin),
_yPin(yPin),
_zPin(zPin),
_biasX(0),
_biasY(0),
_biasZ(0),
_lastSampleTime(0) {}

// begin
// Configures ADC. Assuming using ADC unit 1, 11dB attenuation (0-3.3V range),
// 12-bit resolution, 1.1Vref. Values are consistent with LilyGO T-Beam.
void accelADC::begin() {
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    esp_adc_cal_characterize(
        ADC_UNIT_1,
        ADC_ATTEN_DB_11,
        ADC_WIDTH_BIT_12,
        1100, // 1.1V Vref
        &adc_chars
    );

    calibrate();
}

// calibrate
// Finds bias on ADC dimensions with 1000 samples, using esp_adc_cal library.
// TODO: May need to adjust num. samples and/or add library
void accelADC::calibrate() {
    uint64_t sumX = 0;
    uint64_t sumY = 0;
    uint64_t sumZ = 0;

    for (int i = 0; i < BIAS_SAMPLES; i++) {
        sum_x += esp_adc_cal_raw_to_voltage(analogRead(xPin), &adc_chars);
        sum_y += esp_adc_cal_raw_to_voltage(analogRead(yPin), &adc_chars);
        sum_z += esp_adc_cal_raw_to_voltage(analogRead(zPin), &adc_chars);
        delay(2);
    }

    _biasX = sumX / BIAS_SAMPLES;
    _biasY = sumY / BIAS_SAMPLES;
    _biasZ = sumZ / BIAS_SAMPLES;

    // Serial.println("Calibration completed");
}

// sampleReady
// Forces consistent 200Hz sampling rate. Detecting up to 100Hz via Nyquist, sufficient for earthquake waves.
bool accelADC::sampleReady() {
    uint32_t currTime = micros();

    if (currTime - _lastSampleTime < SAMPLE_PERIOD_US) return false;
    _lastSampleTime = currTime;
    return true;
}

// read
// Gathers sample, adjusting for nonlinear parameters and bias, returning
// struct with x/y/z acceleration in m/s
AccelData accelADC:read() {
    // Read ADC values (voltage) considering nonlinear parameters
    uint32_t x_mv = esp_adc_cal_raw_to_voltage(analogRead(_xPin), &adc_chars);
    uint32_t y_mv = esp_adc_cal_raw_to_voltage(analogRead(_yPin), &adc_chars);
    uint32_t z_mv = esp_adc_cal_raw_to_voltage(analogRead(_zPin), &adc_chars);

    // Bias removal
    float x_centered = (float)x_mv - _biasX;
    float y_centered = (float)y_mv - _biasY;
    float z_centered = (float)z_mv - _biasZ;

    AccelData sample;
    // Convert to acceleration
    sample.ax = (x_centered / ADXL_SENSITIVITY_MV_PER_G) * G_TO_ACCEL;
    sample.ay = (y_centered / ADXL_SENSITIVITY_MV_PER_G) * G_TO_ACCEL;
    sample.az = (z_centered / ADXL_SENSITIVITY_MV_PER_G) * G_TO_ACCEL;

    return sample;
}
