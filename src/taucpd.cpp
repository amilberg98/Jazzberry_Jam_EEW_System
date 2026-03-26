#include "taucpd.h"

#include <cmath>
#include <stdexcept>

namespace {
constexpr float PI_F = 3.14159265358979323846f;
}

float TauCPdEstimator::absfLocal(float x) {
    return (x < 0.0f) ? -x : x;
}

TauCPdEstimator::TauCPdEstimator(float fs_hz, float window_seconds)
    : fs_(fs_hz),
      dt_(0.0f),
      window_samples_(0),
      vel_buf_(),
      disp_buf_(),
      sum_v2_(0.0f),
      sum_d2_(0.0f),
      prev_acc_(0.0f),
      prev_vel_(0.0f),
      velocity_(0.0f),
      displacement_(0.0f),
      idx_(0),
      count_(0),
      tau_c_(0.0f),
      p_d_(0.0f) {
    if (fs_hz <= 0.0f || window_seconds <= 0.0f) {
        throw std::invalid_argument("fs_hz and window_seconds must be positive");
    }

    dt_ = 1.0f / fs_hz;
    window_samples_ = static_cast<std::uint32_t>(window_seconds * fs_hz + 0.5f);

    if (window_samples_ < 2) {
        window_samples_ = 2;
    }

    vel_buf_.assign(window_samples_, 0.0f);
    disp_buf_.assign(window_samples_, 0.0f);

    reset();
}

void TauCPdEstimator::reset() {
    sum_v2_ = 0.0f;
    sum_d2_ = 0.0f;

    prev_acc_ = 0.0f;
    prev_vel_ = 0.0f;
    velocity_ = 0.0f;
    displacement_ = 0.0f;

    idx_ = 0;
    count_ = 0;

    tau_c_ = 0.0f;
    p_d_ = 0.0f;

    for (std::uint32_t i = 0; i < window_samples_; ++i) {
        vel_buf_[i] = 0.0f;
        disp_buf_[i] = 0.0f;
    }
}

void TauCPdEstimator::processSample(float acc_sample) {
    // Remove oldest sample from running sums if window already full
    if (count_ == window_samples_) {
        const float old_v = vel_buf_[idx_];
        const float old_d = disp_buf_[idx_];

        sum_v2_ -= old_v * old_v;
        sum_d2_ -= old_d * old_d;
    }

    // Trapezoidal integration: acceleration -> velocity
    const float new_velocity =
        velocity_ + 0.5f * (acc_sample + prev_acc_) * dt_;

    // Trapezoidal integration: velocity -> displacement
    const float new_displacement =
        displacement_ + 0.5f * (new_velocity + prev_vel_) * dt_;

    // Store in ring buffer
    vel_buf_[idx_] = new_velocity;
    disp_buf_[idx_] = new_displacement;

    // Add newest sample to running sums
    sum_v2_ += new_velocity * new_velocity;
    sum_d2_ += new_displacement * new_displacement;

    // Advance ring buffer index
    ++idx_;
    if (idx_ >= window_samples_) {
        idx_ = 0;
    }

    if (count_ < window_samples_) {
        ++count_;
    }

    // Update integration state
    prev_acc_ = acc_sample;
    prev_vel_ = new_velocity;
    velocity_ = new_velocity;
    displacement_ = new_displacement;

    // Compute P_d = max absolute displacement over current window
    float pd = 0.0f;
    for (std::uint32_t i = 0; i < count_; ++i) {
        const float ad = absfLocal(disp_buf_[i]);
        if (ad > pd) {
            pd = ad;
        }
    }
    p_d_ = pd;

    // Compute tau_c = 2*pi*sqrt(sum(d^2)/sum(v^2))
    if (sum_v2_ > 1e-12f && sum_d2_ >= 0.0f) {
        tau_c_ = 2.0f * PI_F * std::sqrt(sum_d2_ / sum_v2_);
    } else {
        tau_c_ = 0.0f;
    }
}

void TauCPdEstimator::processBuffer(const float* acc, std::size_t n) {
    if (acc == nullptr) {
        return;
    }

    for (std::size_t i = 0; i < n; ++i) {
        processSample(acc[i]);
    }
}

float TauCPdEstimator::getTauC() const {
    return tau_c_;
}

float TauCPdEstimator::getPd() const {
    return p_d_;
}

float TauCPdEstimator::getSampleRate() const {
    return fs_;
}

float TauCPdEstimator::getDt() const {
    return dt_;
}

std::uint32_t TauCPdEstimator::getWindowSamples() const {
    return window_samples_;
}