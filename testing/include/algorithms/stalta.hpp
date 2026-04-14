// STA/LTA (Short-Term Average / Long-Term Average) earthquake detector.
//
// The classic STA/LTA detector computes, at each sample i:
//   STA(i) = mean( |data[i - staLen + 1 .. i]| )
//   LTA(i) = mean( |data[i - ltaLen + 1 .. i]| )
//   ratio  = STA(i) / LTA(i)
//
// A trigger fires when ratio >= threshold.
//
// This implementation optionally accepts a vector of "initial values" that
// are used to pre-seed the LTA before processing begins.  This allows the
// caller to supply a quiet pre-event noise window (e.g. the first N samples
// of a recording that precede the earthquake arrival) so that the detector
// immediately has a meaningful LTA estimate rather than building one up from
// scratch.

#pragma once

#include <vector>

namespace algorithms
{

    struct StaLtaConfig
    {
        int staLen;       // Short-term average window length in samples (must be < ltaLen)
        int ltaLen;       // Long-term  average window length in samples
        double threshold; // STA/LTA ratio that fires a trigger (e.g. 3.0)

        // When true AND initialValues are non-empty, the LTA is frozen to the
        // mean absolute value of the initial-values window rather than updated
        // as a sliding average.  This prevents the onset ramp from contaminating
        // the background noise estimate and makes the trigger fire as soon as the
        // STA window fills with earthquake energy — typically within one STA
        // window length (~0.2 s) of the first arrival.
        bool freezeLta = false;
    };

    struct StaLtaResult
    {
        int triggerIndex = -1;        // Sample index within the processed data where trigger fired
        double triggerTimeSec = -1.0; // Seconds from start of processed data (-1 = no trigger)
        bool triggered = false;
    };

    // Detect the first STA/LTA trigger in `data`.
    //
    // `initialValues`  – optional pre-event samples used to seed the LTA.
    //   • If non-empty, the last min(ltaLen, initialValues.size()) values of
    //     initialValues are used to initialise the LTA circular buffer, and
    //     detection starts at index 0 of `data`.
    //   • If empty (default), the LTA accumulates naturally and detection begins
    //     once the LTA window is fully populated (index ltaLen).
    //
    // The returned StaLtaResult::triggerIndex and ::triggerTimeSec are always
    // expressed relative to the start of `data` (index 0 = time 0).
    StaLtaResult detectEarthquake(
        const std::vector<double> &data,
        double sampleRateHz,
        StaLtaConfig config,
        const std::vector<double> &initialValues = {});

} // namespace algorithms
