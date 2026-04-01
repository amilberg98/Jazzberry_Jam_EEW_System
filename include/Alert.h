#ifndef ALERT_H
#define ALERT_H

#include <Arduino.h>
#include "radio.h"
#include "taucpd.h"

// Handles device and broadcasted (LoRA) alerts, and 
// local error alert.
class Alert {
public:
    Alert(Radio& radioRef, int buzzerPin, TauCPdEstimator& taucpdRef);

    void startLocalAlert(); 
    void stopLocalAlert();  
    void startErrorAlert(); 
    void sendNetworkAlert();
    void recvNetworkAlert();
    void update(); // Check for pending broadcasts and alert timeouts

private:
    Radio& radio;
    int buzzerPin;
    TauCPdEstimator& taucpd;

    // Thresholds for triggering local alert on received values
    const float TAUC_THRESHOLD = 1.0f;
    const float PD_THRESHOLD = 0.5f;

    // Timeout for local alerts
    const unsigned long LOCAL_ALERT_DURATION_MS = 10000;

    // For delayed broadcast after receiving alert
    unsigned long lastRecvTime = 0;
    bool pendingBroadcast = false;

    // For local alert timeout
    unsigned long localAlertStartTime = 0;
    bool localAlertActive = false;
};

#endif