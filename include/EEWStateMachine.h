#ifndef EEWSTATEMACHINE_H
#define EEWSTATEMACHINE_H

#include <Arduino.h>
#include "accelADC.h"
#include "biquad.h"
#include "radio.h"


// Handles main functionality of device, swapping between monitoring, alerting, and error states
class EEWStateMachine
{

public:

    EEWStateMachine(accelADC& accelRef,
                     Biquad& filterRef,
                     Radio& loraRef);

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
    // STALTA& stalta; in the future once implemented

    int alertStartTime;
    const int ALERT_TIMEOUT = 10000;

    void handleMonitoring();
    void handleAlert();
    void handleError();


    void enterMonitoring();
    void enterAlert();
    void enterError();



    // Placeholder functions 
        // In main?
        void checkBattery() {} // Check battery via analogRead voltage divider
        bool errorDetected() {}

        // For future alert class
        void startLocalAlert() {} // Start playing device alert
        void stopLocalAlert() {} // Stop playing device alert
        void startErrorAlert() {} // Alert locally with different tone
        void sendNetworkAlert() {} // Send alert over LoRA
        void recvNetworkAlert() {} // Receive alert over LoRA


    // Placeholder for STA/LTA class
    class STALTA {
        public:
        void append(float sample) {} // Add to STA & LTA samples
        bool thresholdExceeded() {} // Tell if STA/LTA alert threshold exceeded
    };
    
    STALTA stalta;

};

#endif