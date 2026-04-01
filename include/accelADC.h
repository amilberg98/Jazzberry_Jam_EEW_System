#ifndef ACCEL_ADC_H
#define ACCEL_ADC_H

#include <Arduino.h>

struct AccelData {
    float ax;
    float ay;
    float az;
};

// Handles ADC calibration and configuration for accelerometer readings, ensuring
// precision.
class accelADC {
    public:
    accelADC(int xPin, int yPin, int zPin);
    
    void begin();
    bool sampleReady();

    AccelData read();
    bool hasError();
    const float SAMPLE_RATE_HZ = 200.0f;

    private:
    void calibrate();

    int _xPin, _yPin, _zPin;
    uint32_t _biasX, _biasY, _biasZ;
    uint32_t _lastSampleTime;
    bool errorFlag;

    // TODO: Adjust 200Hz sampling rate as necessary (should cover all earthquake waves up to 100Hz)
    const uint32_t SAMPLE_PERIOD_US = (1000000.0f / SAMPLE_RATE_HZ);

    // TODO: Adjust accordingly.
    const uint32_t BIAS_SAMPLES = 1000;
    const float ADXL_SENSITIVITY_MV_PER_G = 1000.0f;
    const float G_TO_ACCEL = 9.80665f;
};

#endif