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


// Handles main functionality of device, swapping between monitoring, alerting, and error states
class EEWStateMachine
{

public:

    EEWStateMachine(accelADC& accelRef,
                     Biquad& filterXRef,
                     Biquad& filterYRef,
                     Biquad& filterZRef,
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
    Biquad& filterX;
    Biquad& filterY;
    Biquad& filterZ;
    Radio& lora;
    STALTA& stalta;
    TauCPdEstimator& taucpd;
    Alert& alert;

    unsigned long alertStartTime = 0;
    unsigned long lastBroadcastTime = 0;
    unsigned long lastAlertEndTime = 0;
    static const unsigned int ALERT_TIMEOUT = 10000;
    static const unsigned long MONITOR_BROADCAST_INTERVAL_MS = 0;

    void handleMonitoring();
    void handleAlert();
    void handleError();


    void enterMonitoring();
    void enterAlert();
    void enterError();



    // Placeholder functions 
        int checkBattery() { // TODO fix doesnt work
            uint32_t raw = analogRead(35);
            return (raw / 1000.0);
        } // Check battery via analogRead voltage divider
        bool errorDetected();

};

#endif