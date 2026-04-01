#include "Alert.h"

// Constructor
// Initializes reference to LoRA radio class and turns off buzzer
Alert::Alert(Radio& radioRef, int buzzerPin, TauCPdEstimator& taucpdRef) : radio(radioRef), buzzerPin(buzzerPin), taucpd(taucpdRef) {
    pinMode(buzzerPin, OUTPUT);
    digitalWrite(buzzerPin, LOW);
}

// startLocalAlert
// Plays buzzer noise and enables red LED for local alert
void Alert::startLocalAlert() {
    // TODO turn on colored LED too like red?
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
    digitalWrite(buzzerPin, HIGH); 
}

// sendNetworkAlert
// Broadcasts earthquake alert over LoRA network with latest TauC and PD values
void Alert::sendNetworkAlert() {
    float tau_c = taucpd.getTauC();
    float p_d = taucpd.getPd();
    String message = "EARTHQUAKE ALERT: TauC=" + String(tau_c, 2) + " PD=" + String(p_d, 2);
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
            
            // Extract TauC and PD from message
            int tauCIndex = received.indexOf("TauC=");
            int pdIndex = received.indexOf(" PD=");
            if (tauCIndex != -1 && pdIndex != -1) {
                String tauCStr = received.substring(tauCIndex + 5, pdIndex);
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
}