#include "STALTA.h"
#include "Arduino.h"

STALTA::STALTA(){
    for (int i = 0; i < STA_LEN; i++) sta_buf[i] = 0;
    for (int i = 0; i < LTA_LEN; i++) lta_buf[i] = 0;
}

STALTA::~STALTA() {
}

// Feed one sample at a time. Returns true if "triggered" after update.
bool STALTA::update(float x) {
    const float e = std::fabs(x);

    // --- update STA rolling sum ---
    sta_sum -= sta_buf[sta_i];
    sta_buf[sta_i] = e;
    sta_sum += e;
    sta_i = (sta_i + 1) % STA_LEN;

    // --- update LTA rolling sum ---
    lta_sum -= lta_buf[lta_i];
    lta_buf[lta_i] = e;
    lta_sum += e;
    lta_i = (lta_i + 1) % LTA_LEN;


    samples_seen++;


    // Not enough history yet, so don't trigger
    if (samples_seen < MIN_LTA_FILL) {
      ratio = 0.0f;
      triggered = false;
      return triggered;
    }

    const float sta = sta_sum / (float)STA_LEN;
    const float lta = lta_sum / (float)LTA_LEN;
    ratio = sta / (lta + ZERO);

    // if (samples_seen % 100 == 0)
    //     Serial.printf("STA: %.2f\n LTA: %.2f\n", sta, lta);


    // Hysteresis
    if (!triggered && ratio >= TRIG_ON) 
        triggered = true;
    else if (triggered && ratio <= TRIG_OFF) 
        triggered = false;

    return triggered;
}

bool STALTA::triggeredStatus() {
 return triggered; 
}



