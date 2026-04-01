#include "Alert.h"

Alert::Alert(Radio& radioRef, int buzzerPin) : radio(radioRef), buzzerPin(buzzerPin) {
    pinMode(buzzerPin, OUTPUT);
    digitalWrite(buzzerPin, LOW); // Ensure buzzer is off initially
}

void Alert::startLocalAlert() {
    digitalWrite(buzzerPin, HIGH); // Drive buzzer pin HIGH to start alert
}

void Alert::stopLocalAlert() {
    digitalWrite(buzzerPin, LOW); // Drive buzzer pin LOW to stop alert
}

void Alert::startErrorAlert() {
    digitalWrite(buzzerPin, HIGH); // Placeholder: Drive buzzer HIGH for error alert (could be different tone in future)
}

void Alert::sendNetworkAlert() {
    // Placeholder: Send a simple alert message over LoRA
    String message = "EARTHQUAKE ALERT";
    uint8_t data[message.length() + 1];
    message.getBytes(data, sizeof(data));
    radio.send(data, message.length());
}

void Alert::recvNetworkAlert() {
    // Placeholder: Check for incoming alert and print it
    if (radio.msgAvailable()) {
        String received = radio.recv();
        if (received.length() > 0) {
            Serial.println("Network Alert Received: " + received);
            // TODO: Handle the alert (e.g., trigger local alert)
        }
    }
}