#ifndef TAUCPD_ESTIMATOR_H
#define TAUCPD_ESTIMATOR_H

#include <cstddef>
#include <cstdint>
#include <vector>

class TauCPdEstimator {
public:
    // fs_hz: sample rate in Hz
    // window_seconds: analysis window length in seconds
    TauCPdEstimator(float fs_hz, float window_seconds);

    // Reset estimator state but keep allocated buffers
    void reset();

    // Process one acceleration sample (m/s^2)
    void processSample(float acc_sample);

    // Process an entire buffer of acceleration samples
    void processBuffer(const float* acc, std::size_t n);

    // Get latest computed values
    float getTauC() const;
    float getPd() const;

    // Basic metadata
    float getSampleRate() const;
    float getDt() const;
    std::uint32_t getWindowSamples() const;

private:
    static float absfLocal(float x);

    // Sampling info
    float fs_;
    float dt_;
    std::uint32_t window_samples_;

    // Sliding-window buffers
    std::vector<float> vel_buf_;
    std::vector<float> disp_buf_;

    // Running sums for tau_c
    float sum_v2_;
    float sum_d2_;

    // Integration state
    float prev_acc_;
    float prev_vel_;
    float velocity_;
    float displacement_;

    // Ring buffer bookkeeping
    std::uint32_t idx_;
    std::uint32_t count_;

    // Latest outputs
    float tau_c_;
    float p_d_;
};

#endif