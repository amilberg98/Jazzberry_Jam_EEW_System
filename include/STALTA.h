#ifndef STALTA_H
#define STALTA_H

#include <cstdint>
#include <cmath>
#include <algorithm>

const int STA_LEN = 200;        // samples
const int LTA_LEN = 4000;       // samples (must be > sta_len)
const float TRIG_ON = 0.5f;     // trigger when ratio >= trig_on
const float TRIG_OFF = 0.2f;    // release when ratio <= trig_off (hysteresis)
const float ZERO = 1e-6f;       // prevent divide-by-zero
const int MIN_LTA_FILL = 500;   // wait until LTA is "warmed up"

// Simple STA/LTA
class STALTA {
public:

  STALTA();
  ~STALTA();
  bool update(float x); 
  bool triggeredStatus();


private:

  float sta_buf[STA_LEN];
  float lta_buf[LTA_LEN];

  int sta_i = 0;
  int lta_i = 0;

  float sta_sum = 0.0f;
  float lta_sum = 0.0f;

  int samples_seen = 0;

  float ratio = 0.0f;
  bool triggered = false;

};

#endif