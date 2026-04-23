#ifndef ALERT_H
#define ALERT_H

#include <Arduino.h>
#include "radio.h"
#include "taucpd.h"

const int BUZZER_PIN = 13;
const int ERR_PIN = 14;
const int ALERT_PIN = 2;
const int EARTHQUAKE_FREQ = 4000; // Frequency for alarm
const int ERROR_FREQ = 2000; // Frequency for error alert

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
    const float TAUC_THRESHOLD = 8.0f;
    const float PD_THRESHOLD = 0.5f;

    // Timeout for local alerts
    const unsigned long LOCAL_ALERT_DURATION_MS = 10000;

    // For delayed broadcast after receiving alert
    unsigned long lastRecvTime = 0;
    bool pendingBroadcast = false;

    // For local alert timeout
    unsigned long localAlertStartTime = 0;
    bool localAlertActive = false;

    // Sender deduplication
    static const int MAX_SEEN_IDS = 8;
    static const unsigned long SEEN_ID_WINDOW_MS = 30000UL;

    struct SeenEntry {
        String id;
        unsigned long timestamp;
    };

    SeenEntry seenIDs[MAX_SEEN_IDS];
    int seenIdx = 0;
    String senderId;
    uint32_t sequenceNumber = 0;

    bool alreadySeen(const String& id);
    void addSeen(const String& id);
};

#endif