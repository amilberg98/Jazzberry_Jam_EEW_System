#ifndef EEWSTATEMACHINE_H
#define EEWSTATEMACHINE_H

#include <Arduino.h>
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

    int alertStartTime;
    const int ALERT_TIMEOUT = 10000;

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