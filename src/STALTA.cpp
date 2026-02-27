#include "STALTA.h"

StaLtaDetector::StaLtaDetector(){
    float sta_buf(STA_LEN);
    float lta_buf(LTA_LEN); 
}

StaLtaDetector::~StaLtaDetector() {
}

// Feed one sample at a time. Returns true if "triggered" after update.
bool StaLtaDetector::update(float x) {
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

    // Hysteresis
    if (!triggered && ratio >= TRIG_ON) 
        triggered = true;
    else if (triggered && ratio <= TRIG_OFF) 
        triggered = false;

    return triggered;
}

bool StaLtaDetector::triggeredStatus() { return triggered; }



