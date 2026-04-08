#include "EEWStateMachine.h"


// Constructor
// Assigns references to components to local variables
EEWStateMachine::EEWStateMachine(accelADC& accelRef, Biquad& filterXRef, Biquad& filterYRef, Biquad& filterZRef, Radio& loraRef, STALTA& staltaRef, TauCPdEstimator& taucpdRef, Alert& alertRef):
accel(accelRef),
filterX(filterXRef),
filterY(filterYRef),
filterZ(filterZRef),
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
                        handleAlert();
                        break;
                case State::ERROR:
                        handleError();
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

                // Filter each axis
                float fx = filterX.process(data.ax);
                float fy = filterY.process(data.ay);
                float fz = filterZ.process(data.az);

                // Compute and use vector magnitude of acceleration on all axes
                float accel_vec = sqrtf(fx*fx + fy*fy + fz*fz);
                
                // Only use one axis for TauC/Pd to prevent infinite accumulation
                taucpd.processSample(fz);
                stalta.update(accel_vec);

                // Check for local alert trigger: STA/LTA must be triggered
                // TODO: Maybe use tauc/pd in trigger also?
                if (millis() - lastAlertEndTime > 10000 && stalta.triggeredStatus() /*&& taucpd.getTauC() > 3.0f && taucpd.getPd() > 0.21f*/) {
                        enterAlert();
                        return;
                }
        
                // Broadcast values periodically to avoid spamming LoRA
                if (millis() - lastBroadcastTime >= MONITOR_BROADCAST_INTERVAL_MS) {
                        char json[256];
                        sprintf(json, "{\"accel_vec\": %.2f,\"filteredX\": %.2f,\"filteredY\": %.2f,\"filteredZ\": %.2f,\"sta\": %.2f,\"lta\": %.2f,\"tauc\": %.2f,\"pd\": %.2f}", accel_vec, fx, fy, fz, stalta.getSTA(), stalta.getLTA(), taucpd.getTauC(), taucpd.getPd());
                        Serial.printf("%s\n", json);
                        Serial.printf("Ratio STATUS: %d\n", stalta.triggeredStatus());
                        lora.send((uint8_t*)json, strlen(json));
                        lastBroadcastTime = millis();
                }
        }

        alert.recvNetworkAlert();
        float voltage = checkBattery();
}

// handleAlert
// Checks for errors, sends LoRA network alert for timeout amount of time.
// TODO: Add functionality to triger local alert.
void EEWStateMachine::handleAlert() {
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
  lastAlertEndTime = millis();
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
