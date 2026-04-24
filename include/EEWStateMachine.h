#ifndef EEWSTATEMACHINE_H
#define EEWSTATEMACHINE_H

#include <Arduino.h>
#include "String.h"
#include "accelADC.h"
#include "biquad.h"
#include "radio.h"
#include "STALTA.h"
#include "taucpd.h"
#include "Alert.h"
#include <Wire.h>
#include <XPowersLib.h>


// Handles main functionality of device, swapping between monitoring, alerting, and error states
class EEWStateMachine
{

public:

    EEWStateMachine(accelADC& accelRef,
                     Biquad& filterRef,
                     Radio& loraRef,
                     STALTA& staltaRef,
                     TauCPdEstimator& taucpdRef,
                     Alert& alertRef);

    void begin();
    void update();

private:
    enum class State
    {
        MONITORING,
        ALERT,
        ERROR
    };
    State currState;

    accelADC& accel;
    Biquad& filter;
    Radio& lora;
    STALTA& stalta;
    TauCPdEstimator& taucpd;
    Alert& alert;
    XPowersAXP2101 power;

    unsigned long alertStartTime = 0;
    unsigned long lastBroadcastTime = 0;
    unsigned long lastAlertEndTime = 0;
    static const unsigned int ALERT_TIMEOUT = 10000;
    static const unsigned long MONITOR_BROADCAST_INTERVAL_MS = 0;
    unsigned long taucpd_window_start = 0;
    
    // Two-phase alert validation
    const unsigned long STA_LTA_CONFIRM_MS = 300;   // Wait 0.3sec for STA/LTA confirmation
    const unsigned long TAUCPD_EVAL_MS = 2000;       // Then collect 2.5sec of tauc/Pd
    
    // MORE SENSITIVE LESS DELAY
    const float TAUC_THRESH = 0.60f;
    const float PD_THRESH = 0.001f;   

    // LESS SENSITIVE MORE DELAY
    // const float TAUC_THRESH = 2.00f;
    // const float PD_THRESH = 0.000100f;

    enum AlertPhase {
        IDLE,                  // Waiting for initial STA/LTA trigger
        CONFIRMING_STA_LTA,    // STA/LTA above threshold, confirming for 0.3sec
        COLLECTING_TAUCPD     // Past 0.3sec, now collecting tauc/Pd for 2.5sec
    };
    
    AlertPhase alert_phase = IDLE;
    unsigned long trigger_time = 0;
    float max_tauC = 0.0f;
    float max_Pd = 0.0f;




    void handleMonitoring();
    void handleAlert();
    void handleError();


    void enterMonitoring();
    void enterAlert();
    void enterError();


    float checkBattery();
    bool errorDetected();

};

#endif
