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
                        handleAlert();
                        break;
                case State::ERROR:
                        handleError();
                        break;

        }
}

// void EEWStateMachine::runFilterLog(int numSamples) {
//     float* rawLog  = new float[numSamples];
//     float* filtLog = new float[numSamples];

//     Serial.printf("Collecting %d samples...\n", numSamples);

//     for (int i = 0; i < numSamples; i++) {
//         while (!accel.sampleReady()); // busy wait, no Serial in here
//         AccelData data = accel.read();
//         float fx = filterX.process(data.ax);
//         float fy = filterY.process(data.ay);
//         float fz = filterZ.process(data.az);
//         rawLog[i] = data.ay;
//         filtLog[i] = fy;
//         //rawLog[i]  = sqrtf(data.ax*data.ax + data.ay*data.ay + data.az*data.az);
//         //filtLog[i] = sqrtf(fx*fx + fy*fy + fz*fz);
//     }

//     // Dump after collection — timing no longer matters here
//     Serial.println("RAW,FILTERED");
//     for (int i = 0; i < numSamples; i++) {
//         Serial.printf("%.6f,%.6f\n", rawLog[i], filtLog[i]);
//     }

//     delete[] rawLog;
//     delete[] filtLog;
//     Serial.println("DONE");
// }

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

                // Filter z axis
                float fz = filter.process(data.az);
                stalta.update(fz);
                
                // TODO: keep debugging, if fails go back to original 2-phase alert (prev commit)--took 3-5sec but worked consistently
                // Parallelized two-phase alert
                // Phase 1: Confirm STA/LTA above threshold for 0.3 ms
                // Phase 2: Evaluate TauC/Pd over same time window (already collecting)
                unsigned long now = millis();

                if (alert_phase == CONFIRMING_STA_LTA &&
                now - trigger_time >= STA_LTA_CONFIRM_MS) {

                        alert_phase = COLLECTING_TAUCPD;

                        taucpd.reset();
                        max_tauC = 0.0f;
                        max_Pd = 0.0f;

                        taucpd_window_start = now;  // IMPORTANT
                }

                // Accumulate/collect tauc and pd
                taucpd.processSample(fz);
                float tau = taucpd.getTauC();
                float pd  = taucpd.getPd();


                // Track rolling max (only when active)
                if (alert_phase != IDLE) {
                        if (tau > max_tauC) max_tauC = tau;
                        if (pd  > max_Pd)   max_Pd   = pd;
                }

                if (alert_phase == IDLE &&
                (now - lastAlertEndTime > 10000) &&
                stalta.triggeredStatus()) {
                        // Start detection window
                        alert_phase = CONFIRMING_STA_LTA;
                        trigger_time = now;

                        // Initialize max trackers
                        max_tauC = tau;
                        max_Pd   = pd;
                }

                if (alert_phase == CONFIRMING_STA_LTA) {
                        if (!stalta.triggeredStatus()) {
                                // Reset if it drops (you can add hysteresis later)
                                alert_phase = IDLE;
                        }
                        else if (now - trigger_time >= STA_LTA_CONFIRM_MS) {
                                // Move to evaluation phase (but TauC/Pd already collected)
                                alert_phase = COLLECTING_TAUCPD;
                        }
                }

                if (alert_phase == COLLECTING_TAUCPD) {
                        taucpd.processSample(fz);

                        float tau = taucpd.getTauC();
                        float pd  = taucpd.getPd();

                        if (tau > max_tauC) max_tauC = tau;
                        if (pd  > max_Pd)   max_Pd = pd;

                        if (now - taucpd_window_start >= TAUCPD_EVAL_MS) {

                                if (max_tauC > TAUC_THRESH && max_Pd > PD_THRESH) {
                                enterAlert();
                                }

                                alert_phase = IDLE;
                        }
                        // // Total window = confirm time + eval time
                        // if (now - trigger_time >= (STA_LTA_CONFIRM_MS + TAUCPD_EVAL_MS)) {

                        //         // Check thresholds using already-collected data
                        //         if (max_tauC > TAUC_THRESH && max_Pd > PD_THRESH) {
                        //                 enterAlert();
                        //         }

                        //         alert_phase = IDLE;
                        // }
                }
                
        
                // Broadcast values periodically to avoid spamming LoRA
                if (millis() - lastBroadcastTime >= MONITOR_BROADCAST_INTERVAL_MS) {
                        char json[256];
                        sprintf(json, "{\"state\":\"M\" \"raw_z\": %.6f, \"filtered_z\": %.6f,\"sta\": %.6f,\"lta\": %.6f,\"tauc\": %.6f,\"pd\": %.6f, \"ratio\": %.2f}", data.az, fz, stalta.getSTA(), stalta.getLTA(), taucpd.getTauC(), taucpd.getPd(), stalta.ratio);
                        Serial.printf("%s\n", json);
                        lora.send((uint8_t*)json, strlen(json));
                        lastBroadcastTime = millis();
                }
        }

        alert.recvNetworkAlert();
        float voltage = checkBattery();
}

// handleAlert
// Checks for errors, sends LoRA network alert for timeout amount of time,
// and continues broadcasting sensor data.
void EEWStateMachine::handleAlert() {
        if (errorDetected()) {
                enterError();
                return;
        }

        if (accel.sampleReady()) {
                AccelData data = accel.read();

                // Filter z axis
                float fz = filter.process(data.az);

                // Only use one axis for TauC/Pd and STA/LTA
                taucpd.processSample(fz);
                stalta.update(fz);

                // Broadcast values periodically to avoid spamming LoRA
                // this part lastBroadcastTime ignored because messes up main logic to end alert
                if (millis() - lastBroadcastTime >= MONITOR_BROADCAST_INTERVAL_MS) {
                        char json[256];
                        sprintf(json, "{\"state\":\"A\" \"raw_z\": %.6f, \"filtered_z\": %.6f,\"sta\": %.6f,\"lta\": %.6f,\"tauc\": %.6f,\"pd\": %.6f, \"ratio\": %.2f}", data.az, fz, stalta.getSTA(), stalta.getLTA(), taucpd.getTauC(), taucpd.getPd(), stalta.ratio);                        Serial.printf("%s\n", json);
                        lora.send((uint8_t*)json, strlen(json));
                        //lastBroadcastTime = millis();
                }
        }

        alert.sendNetworkAlert();

        if (millis() - alertStartTime > ALERT_TIMEOUT) {
                stalta.ratio = 0.0f; // Reset STA/LTA to avoid same alarm twice
                taucpd.reset();
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
