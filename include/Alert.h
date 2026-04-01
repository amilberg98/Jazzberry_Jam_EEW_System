#ifndef ALERT_H
#define ALERT_H

#include <Arduino.h>
#include "radio.h"

class Alert {
public:
    Alert(Radio& radioRef, int buzzerPin);

    void startLocalAlert(); // Start playing device alert (drive buzzer pin HIGH)
    void stopLocalAlert();  // Stop playing device alert (drive buzzer pin LOW)
    void startErrorAlert(); // Alert locally with different tone (placeholder: drive buzzer HIGH)
    void sendNetworkAlert(); // Send alert over LoRA
    void recvNetworkAlert(); // Receive alert over LoRA (placeholder: check and print)

private:
    Radio& radio;
    int buzzerPin;
};

#endif