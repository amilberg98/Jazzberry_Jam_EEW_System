#include "EEWStateMachine.h"


// Constructor
// Assigns references to components to local variables
EEWStateMachine::EEWStateMachine(accelADC& accelRef, Biquad& filterRef, Radio& loraRef, STALTA& staltaRef, TauCPdEstimator& taucpdRef, Alert& alertRef):
accel(accelRef),
filter(filterRef),
lora(loraRef),
stalta(staltaRef),
taucpd(taucpdRef),
alert(alertRef)
 {};

// begin
// Starts LoRA listening and begins in Monitoring state
void EEWStateMachine::begin() {
        currState = State::MONITORING;
        lora.startListen();
}

// update
// Main switch statement for state machine, switching between appropriate handlers
void EEWStateMachine::update() {
        alert.update(); // Check for pending alerts
        switch(currState) {
                case State::MONITORING:
                        handleMonitoring();
                        break;
                case State::ALERT:
                        handleMonitoring();
                        break;
                case State::ERROR:
                        handleMonitoring();
                        break;

        }
}

/************** State Handling Functions ***************/

// handleMonitoring
// Checks for errors, gets accelerometer sample, filters and appends to STA/LTA,
// and if STA/LTA threshold exceeded triggers Alert state. Also checks for network
// alert and for low battery.
void EEWStateMachine::handleMonitoring() {
        if (errorDetected()) {
                enterError();
                return;
        }


        if (accel.sampleReady()) {
                AccelData data = accel.read();
                float filtered = filter.process(data.ay); // TODO: Use different axes? or only y?
                taucpd.processSample(data.ay); // TODO above
                Serial.printf("%.2f\n%.2f\n", data.ay, filtered);
                stalta.update(filtered);

                if (stalta.triggeredStatus()) {
                        enterAlert();
                        return;
                }
        }

        alert.recvNetworkAlert();
        float voltage = checkBattery();
}

// handleAlert
// Checks for errors, sends LoRA network alert for timeout amount of time.
// TODO: Add functionality to triger local alert.
void EEWStateMachine::handleAlert() {
        Serial.printf("ALERT\n");
        if (errorDetected()) {
                enterError();
                return;
        }

        alert.sendNetworkAlert();

        if (millis() - alertStartTime > ALERT_TIMEOUT) {
                enterMonitoring();
        }
}

// handleError
// Runs unique error alert tone. TODO: Implement error alert tone.
void EEWStateMachine::handleError() {
        alert.startErrorAlert();
}


/************** State Transition Functions ***************/

// enterAlert
// Turns off radio (puts in standby mode), gets alert start time, and
// starts alert
void EEWStateMachine::enterAlert() {
  currState = State::ALERT;
  alertStartTime = millis();
  alert.startLocalAlert();
  lora.standby(); 
}

// enterMonitoring
// Stops alert on device, restarts radio, and enters Monitoring
void EEWStateMachine::enterMonitoring() {
  currState = State::MONITORING;
  alert.stopLocalAlert();
  lora.startListen();
}

// Starts error alert tone and enters Error state
void EEWStateMachine::enterError() {
  currState = State::ERROR;
  alert.startErrorAlert();
}

bool EEWStateMachine::errorDetected() {
    return accel.hasError() || lora.hasError();
}
