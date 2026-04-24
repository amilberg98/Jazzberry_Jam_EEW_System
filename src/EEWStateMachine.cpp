#include "EEWStateMachine.h"

// Constructor
// Assigns references to components to local variables
EEWStateMachine::EEWStateMachine(accelADC &accelRef, Biquad &filterRef, Radio &loraRef, STALTA &staltaRef, TauCPdEstimator &taucpdRef, Alert &alertRef) : accel(accelRef),
                                                                                                                                                          filter(filterRef),
                                                                                                                                                          lora(loraRef),
                                                                                                                                                          stalta(staltaRef),
                                                                                                                                                          taucpd(taucpdRef),
                                                                                                                                                          alert(alertRef) {};

// begin
// Starts LoRA listening and begins in Monitoring state
void EEWStateMachine::begin()
{
    currState = State::MONITORING;
    lora.startListen();
}

// update
// Main switch statement for state machine, switching between appropriate handlers
void EEWStateMachine::update()
{
    alert.update(); // Check for pending alerts
    switch (currState)
    {
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

// checkBattery
// Checks the battery level to see if too low and trigger error
float EEWStateMachine::checkBattery() {
  return (power.getBattVoltage() / 1000.0);
}

/************** State Handling Functions ***************/

// handleMonitoring
// Checks for errors, gets accelerometer sample, filters and appends to STA/LTA,
// and if STA/LTA threshold exceeded triggers Alert state. Also checks for network
// alert and for low battery.
void EEWStateMachine::handleMonitoring()
{
    if (errorDetected())
    {
        enterError();
        return;
    }

    if (accel.sampleReady())
    {
        AccelData data = accel.read();

        // Filter z axis
        float fz = filter.process(data.az);
        stalta.update(fz);

        // TWO-PHASE ALERT LOGIC
        // Phase 1: Confirm STA/LTA above threshold for 0.3sec
        // Phase 2: Evaluate tauc/Pd during the 2.5sec window after phase 1
        unsigned long now = millis();

        if (alert_phase == IDLE && (now - lastAlertEndTime > 10000) && stalta.triggeredStatus())
        {
            // Start phase 1: STA/LTA confirmation
            alert_phase = CONFIRMING_STA_LTA;
            trigger_time = now;
        }

        if (alert_phase == CONFIRMING_STA_LTA)
        {
            if (!stalta.triggeredStatus())
            {
                // STA/LTA dropped below threshold, restart
                alert_phase = IDLE;
            }
            else if (now - trigger_time >= STA_LTA_CONFIRM_MS)
            {
                // Phase 1 complete, move to phase 2: collect 3sec of tauc/Pd
                alert_phase = COLLECTING_TAUCPD;
                taucpd.reset();
                max_tauC = 0.0f;
                max_Pd = 0.0f;
            }
        }

        if (alert_phase == COLLECTING_TAUCPD)
        {
            taucpd.processSample(fz);
            float tau = taucpd.getTauC();
            float pd = taucpd.getPd();
            Serial.printf("COLLECTING: MaxTauC: %.6f MaxPd: %.6f\r\n", tau, pd);

            if (tau > max_tauC)
                max_tauC = tau;
            if (pd > max_Pd)
                max_Pd = pd;

            if (now - trigger_time >= (STA_LTA_CONFIRM_MS + TAUCPD_EVAL_MS))
            {
                //  Phase 2 complete, check if both thresholds exceeded
                if (max_tauC > TAUC_THRESH && max_Pd > PD_THRESH)
                {
                    enterAlert();
                }
                alert_phase = IDLE;
            }
        }


        // Setup JSON values for serial monitor, print with delay (0 currently, full speed)
        if (millis() - lastBroadcastTime >= MONITOR_BROADCAST_INTERVAL_MS)
        {
            char json[256];
            sprintf(json, "{\"state\":\"M\", \"raw_z\": %.6f, \"filtered_z\": %.6f,\"sta\": %.6f,\"lta\": %.6f,\"tauc\": %.6f,\"pd\": %.6f, \"ratio\": %.2f}", data.az, fz, stalta.getSTA(), stalta.getLTA(), taucpd.getTauC(), taucpd.getPd(), stalta.ratio);
            Serial.printf("%s\n", json);
        }
    }

    alert.recvNetworkAlert();
    float voltage = checkBattery();
}

// handleAlert
// Checks for errors, sends LoRA network alert for timeout amount of time,
// and continues broadcasting sensor data.
void EEWStateMachine::handleAlert()
{
    if (errorDetected())
    {
        enterError();
        return;
    }

    if (accel.sampleReady())
    {
        AccelData data = accel.read();

        // Filter z axis
        float fz = filter.process(data.az);

        // Only use one axis for TauC/Pd and STA/LTA
        taucpd.processSample(fz);
        stalta.update(fz);

        // Keep sending values to UI
        if (millis() - lastBroadcastTime >= MONITOR_BROADCAST_INTERVAL_MS)
        {
            char json[256];
            sprintf(json, "{\"state\":\"A\", \"raw_z\": %.6f, \"filtered_z\": %.6f,\"sta\": %.6f,\"lta\": %.6f,\"tauc\": %.6f,\"pd\": %.6f, \"ratio\": %.2f}", data.az, fz, stalta.getSTA(), stalta.getLTA(), taucpd.getTauC(), taucpd.getPd(), stalta.ratio);
            Serial.printf("%s\n", json);
        }
    }

    alert.sendNetworkAlert();

    if (millis() - alertStartTime > ALERT_TIMEOUT)
    {
        stalta.ratio = 0.0f; // Reset STA/LTA to avoid same alarm twice
        taucpd.reset();
        enterMonitoring();
    }
}

// handleError
// Runs unique error alert tone. TODO: Implement error alert tone.
void EEWStateMachine::handleError()
{
    alert.startErrorAlert();
}

/************** State Transition Functions ***************/

// enterAlert
// Turns off radio (puts in standby mode), gets alert start time, and
// starts alert
void EEWStateMachine::enterAlert()
{
    currState = State::ALERT;
    alertStartTime = millis();
    alert.startLocalAlert();
    lora.standby();
}

// enterMonitoring
// Stops alert on device, restarts radio, and enters Monitoring
void EEWStateMachine::enterMonitoring()
{
    currState = State::MONITORING;
    lastAlertEndTime = millis();
    alert.stopLocalAlert();
    lora.startListen();
}

// Starts error alert tone and enters Error state
void EEWStateMachine::enterError()
{
    currState = State::ERROR;
    alert.startErrorAlert();
}


bool EEWStateMachine::errorDetected()
{
    return accel.hasError() || lora.hasError();
}
