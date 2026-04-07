#include "Alert.h"
#include <WiFi.h>

// Constructor
// Initializes reference to LoRA radio class and turns off buzzer
Alert::Alert(Radio& radioRef, int buzzerPin, TauCPdEstimator& taucpdRef) : radio(radioRef), buzzerPin(buzzerPin), taucpd(taucpdRef) {
    pinMode(buzzerPin, OUTPUT);
    digitalWrite(buzzerPin, LOW);
    senderId = WiFi.macAddress();
    addSeen(senderId);
}

// startLocalAlert
// Plays buzzer noise and enables red LED for local alert
void Alert::startLocalAlert() {
    // TODO turn on colored LED too like red?
    // TODO: Use tone(buzzerPin, EARTHQUAKE_FREQ) with passive buzzer for unique alert
    digitalWrite(buzzerPin, HIGH);
    localAlertStartTime = millis();
    localAlertActive = true;
}

// stopLocalAlert
// Disables buzzer for local alert
void Alert::stopLocalAlert() {
    digitalWrite(buzzerPin, LOW);
    localAlertActive = false;
}

// startErrorAlert
// Plays buzzer noise and enables yellow LED for error alert
void Alert::startErrorAlert() {
    // TODO turn on different colored LED
    // TODO: Use tone(buzzerPin, ERROR_FREQ) with passive buzzer for unique alert
    digitalWrite(buzzerPin, HIGH); 
}

// sendNetworkAlert
// Broadcasts earthquake alert over LoRA network with latest TauC and PD values
void Alert::sendNetworkAlert() {
    float tau_c = taucpd.getTauC();
    float p_d = taucpd.getPd();
    sequenceNumber += 1;

    String message = "EARTHQUAKE ALERT: ID=" + senderId + " SEQ=" + String(sequenceNumber);
    message += " TauC=" + String(tau_c, 2);
    message += " PD=" + String(p_d, 2);

    addSeen(senderId);

    uint8_t data[message.length() + 1];
    message.getBytes(data, sizeof(data));
    radio.send(data, message.length());
}

// recvNetworkAlert
// Checks for and receives any incoming alert
// Extract TauC and PD if present, checking if they exceed our threshold.
// If exceeded, waits 2sec then broadcasts own TauC/Pd values
void Alert::recvNetworkAlert() {
    if (radio.msgAvailable()) {
        String received = radio.recv();
        if (received.length() > 0) {
            Serial.println("Network Alert Received: " + received);
            
            int idIndex = received.indexOf("ID=");
            int seqIndex = received.indexOf(" SEQ=");
            int tauCIndex = received.indexOf(" TauC=");
            int pdIndex = received.indexOf(" PD=");

            if (idIndex != -1 && seqIndex != -1 && tauCIndex != -1 && pdIndex != -1) {
                String sender = received.substring(idIndex + 3, seqIndex);
                if (alreadySeen(sender)) {
                    Serial.println("Duplicate alert ignored: " + sender);
                    return;
                }
                addSeen(sender);

                String tauCStr = received.substring(tauCIndex + 6, pdIndex);
                String pdStr = received.substring(pdIndex + 4);
                float tauC = tauCStr.toFloat();
                float pd = pdStr.toFloat();
                Serial.printf("Extracted TauC: %.2f, PD: %.2f\n", tauC, pd);
                
                // Check thresholds and cascade alert if exceeded
                if (tauC > TAUC_THRESHOLD && pd > PD_THRESHOLD) {
                    Serial.println("Thresholds exceeded, triggering local alert and scheduling broadcast");
                    startLocalAlert();
                    pendingBroadcast = true;
                    lastRecvTime = millis();
                }
            }
        }
    }
}

// update
// Checks for pending broadcast alert and if local alert is happening.
// On broadcast alert, sends own alert after 2sec.
// On local alert, ends it after 10sec.
void Alert::update() {
    if (pendingBroadcast && (millis() - lastRecvTime > 2000)) {
        Serial.println("Broadcasting own TauC and PD after delay");
        sendNetworkAlert();
        pendingBroadcast = false;
    }

    if (localAlertActive && (millis() - localAlertStartTime > LOCAL_ALERT_DURATION_MS)) {
        Serial.println("Local alert timeout, stopping alert");
        stopLocalAlert();
    }
    // TODO use below with passive buzzer
    // if (localAlertActive) {
    //     unsigned long now = millis();
    //     if (now - localAlertStartTime > LOCAL_ALERT_DURATION_MS) {
    //         stopLocalAlert();
    //     } else {
    //         // Alternate tone on/off every BEEP_INTERVAL_MS for urgency
    //         bool beepOn = ((now - localAlertStartTime) / BEEP_INTERVAL_MS) % 2 == 0;
    //         if (beepOn) {
    //             tone(buzzerPin, EARTHQUAKE_FREQ);
    //         } else {
    //             noTone(buzzerPin);
    //         }
    //     }
    // }
}

bool Alert::alreadySeen(const String& id) {
    unsigned long now = millis();
    for (int i = 0; i < MAX_SEEN_IDS; ++i) {
        if (seenIDs[i].id == id) {
            if (now - seenIDs[i].timestamp <= SEEN_ID_WINDOW_MS) {
                return true;
            }
            break;
        }
    }
    return false;
}

void Alert::addSeen(const String& id) {
    seenIDs[seenIdx].id = id;
    seenIDs[seenIdx].timestamp = millis();
    seenIdx = (seenIdx + 1) % MAX_SEEN_IDS;
}