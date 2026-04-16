#ifndef STALTA_H
#define STALTA_H

#include <cstdint>
#include <cmath>
#include <algorithm>

// TODO change tauc/pd also and tets this next time. Maybe use just fz for STA/LTA?

const int STA_LEN = 200;        // samples
const int LTA_LEN = 6000;       // samples (must be > sta_len)
const float TRIG_ON = 4.0f;     // trigger when ratio >= trig_on
const float TRIG_OFF = 3.0f;    // release when ratio <= trig_off (hysteresis)
const float ZERO = 0.0001f;       // prevent divide-by-zero
const int MIN_LTA_FILL = LTA_LEN / 10;   // wait until LTA is "warmed up"

// Simple STA/LTA
class STALTA {
public:

  STALTA();
  ~STALTA();
  bool update(float x); 
  bool triggeredStatus();
  float getSTA();
  float getLTA();

  float ratio = 0.0f;


private:

  float sta_buf[STA_LEN];
  float lta_buf[LTA_LEN];

  int sta_i = 0;
  int lta_i = 0;

  float sta_sum = 0.0f;
  float lta_sum = 0.0f;

  int samples_seen = 0;


  bool triggered = false;

};

#endif