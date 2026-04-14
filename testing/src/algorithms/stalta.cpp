#include "algorithms/stalta.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace algorithms
{

    StaLtaResult detectEarthquake(
        const std::vector<double> &data,
        double sampleRateHz,
        StaLtaConfig config,
        const std::vector<double> &initialValues)
    {
        if (sampleRateHz <= 0.0)
        {
            throw std::invalid_argument("sampleRateHz must be positive");
        }
        if (config.staLen <= 0 || config.ltaLen <= 0)
        {
            throw std::invalid_argument("STA and LTA window lengths must be positive");
        }
        if (config.staLen >= config.ltaLen)
        {
            throw std::invalid_argument("STA window must be shorter than LTA window");
        }
        if (config.threshold <= 0.0)
        {
            throw std::invalid_argument("threshold must be positive");
        }

        // -----------------------------------------------------------------------
        // Build a combined array:
        //   combined = [last min(ltaLen, n_init) values of initialValues] ++ data
        //
        // By prepending up to ltaLen initial values, the LTA window is fully
        // populated the moment we begin processing data[0], and detection can
        // start immediately at the boundary rather than waiting for ltaLen new
        // samples to accumulate.
        // -----------------------------------------------------------------------

        const int n_init_available = static_cast<int>(initialValues.size());
        const int n_seed = std::min(n_init_available, config.ltaLen);

        // Number of elements prepended from initialValues
        const int seedStart = n_init_available - n_seed; // index in initialValues

        const int n_data = static_cast<int>(data.size());
        const int n_combined = n_seed + n_data;

        if (n_combined < config.ltaLen)
        {
            // Not enough data to compute even one LTA value.
            return StaLtaResult{};
        }

        // Build combined absolute-value array
        std::vector<double> absVals(n_combined);
        for (int i = 0; i < n_seed; ++i)
        {
            absVals[i] = std::abs(initialValues[seedStart + i]);
        }
        for (int i = 0; i < n_data; ++i)
        {
            absVals[n_seed + i] = std::abs(data[i]);
        }

        // Build prefix sums: prefixSum[i] = sum of absVals[0..i-1]
        // prefixSum has size n_combined + 1
        std::vector<double> prefixSum(n_combined + 1, 0.0);
        for (int i = 0; i < n_combined; ++i)
        {
            prefixSum[i + 1] = prefixSum[i] + absVals[i];
        }

        // Window sum helper: sum of absVals[lo..hi] (inclusive)
        auto windowSum = [&](int lo, int hi) -> double
        {
            return prefixSum[hi + 1] - prefixSum[lo];
        };

        // -----------------------------------------------------------------------
        // Optionally freeze the LTA at the mean of the initial-values window.
        // When freezeLta is set, we compute a single background level from ALL
        // provided initial values (not just the ltaLen-capped tail that was
        // prepended to the combined array).  This ensures the frozen level
        // matches exactly what the caller computed as its baseline window mean,
        // regardless of whether n_init_available > ltaLen.
        // -----------------------------------------------------------------------
        double frozenLtaValue = 0.0;
        if (config.freezeLta && n_init_available > 0)
        {
            double sum = 0.0;
            for (const auto &v : initialValues)
            {
                sum += std::abs(v);
            }
            frozenLtaValue = sum / n_init_available;
        }

        // -----------------------------------------------------------------------
        // Scan from the first position where both windows are fully populated.
        // The STA window [i - staLen + 1 .. i] is full when i >= staLen - 1.
        // The LTA window [i - ltaLen + 1 .. i] is full when i >= ltaLen - 1.
        // With n_seed prepended, the first sample of `data` sits at combined
        // index n_seed.  We guarantee the LTA is full from combined index
        // ltaLen - 1 onwards.  Since n_seed <= ltaLen, the LTA is fully
        // populated for all combined indices >= ltaLen - 1.
        // -----------------------------------------------------------------------

        const int scanStart = std::max(n_seed, config.ltaLen - 1);

        StaLtaResult result;

        for (int ci = scanStart; ci < n_combined; ++ci)
        {
            double lta;
            if (config.freezeLta && frozenLtaValue > 0.0)
            {
                lta = frozenLtaValue;
            }
            else
            {
                lta = windowSum(ci - config.ltaLen + 1, ci) / config.ltaLen;
            }

            if (lta <= 0.0)
            {
                continue;
            }

            const int staLo = std::max(0, ci - config.staLen + 1);
            const int staCount = ci - staLo + 1;
            const double sta = windowSum(staLo, ci) / staCount;

            const double ratio = sta / lta;

            if (ratio >= config.threshold)
            {
                // ci is a combined index; map back to data index.
                const int dataIndex = ci - n_seed;
                result.triggered = true;
                result.triggerIndex = dataIndex;
                result.triggerTimeSec = dataIndex / sampleRateHz;
                return result;
            }
        }

        return result;
    }

} // namespace algorithms
