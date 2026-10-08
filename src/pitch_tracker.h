#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

// Estimates the speaker's fundamental frequency (F0) with the YIN algorithm and keeps a slowly moving
// average of it. The average describes the *voice* (deep, high...), not the current intonation, which is
// what a voice changer needs: shift by a constant ratio so the melody of speech survives.
class PitchTracker {
public:
    static constexpr int kRate = 16000;            // analysis rate; the input is decimated 3:1 from 48 kHz
    static constexpr int kWindow = 640;            // 40 ms integration window
    static constexpr int kMinF0 = 65, kMaxF0 = 450;

    void reset() {
        std::memset(buf_, 0, sizeof(buf_));
        fill_ = 0;
        voicedCount_ = 0;
        histCount_ = 0;
        avgLog2_ = std::log2(125.0f);  // until we know better, assume an average male voice
        hasAverage_ = false;
    }

    // Feeds one block of 48 kHz samples (a multiple of 3) and updates the estimate.
    void push(const float* x, int n) {
        for (int i = 0; i + 2 < n; i += 3) {
            const float s = (x[i] + x[i + 1] + x[i + 2]) * (1.0f / 3.0f);  // crude anti-alias, fine for F0
            if (fill_ == kBuf) {
                std::memmove(buf_, buf_ + (kBuf - kKeep), sizeof(float) * kKeep);
                fill_ = kKeep;
            }
            buf_[fill_++] = s;
        }
        if (fill_ >= kKeep) analyse();
    }

    // Smoothed F0 of the speaker in Hz (a guess of 125 Hz until speech has been heard).
    float averageF0() const { return std::exp2(avgLog2_); }
    bool hasAverage() const { return hasAverage_; }
    // F0 of the last analysed frame, 0 if it was not voiced.
    float currentF0() const { return current_; }

private:
    static constexpr int kMaxTau = kRate / kMinF0 + 2;
    static constexpr int kMinTau = kRate / kMaxF0;
    static constexpr int kKeep = kWindow + kMaxTau;  // samples needed per analysis
    static constexpr int kBuf = kKeep + 160 * 4;

    void analyse() {
        const float* x = buf_ + (fill_ - kKeep);

        // Silence is never voiced.
        double energy = 0;
        for (int j = 0; j < kWindow; ++j) energy += double(x[j]) * x[j];
        current_ = 0;
        if (std::sqrt(energy / kWindow) < 0.004) return;

        // Difference function and cumulative mean normalisation (YIN steps 2 and 3).
        float d[kMaxTau + 1];
        d[0] = 1.0f;
        double running = 0;
        for (int tau = 1; tau <= kMaxTau; ++tau) {
            double sum = 0;
            for (int j = 0; j < kWindow; ++j) {
                const double diff = double(x[j]) - x[j + tau];
                sum += diff * diff;
            }
            running += sum;
            d[tau] = running > 0 ? static_cast<float>(sum * tau / running) : 1.0f;
        }

        // Absolute threshold (step 4): first dip below the threshold, followed down to its minimum.
        int best = 0;
        for (int tau = kMinTau; tau < kMaxTau; ++tau) {
            if (d[tau] < 0.15f) {
                while (tau + 1 < kMaxTau && d[tau + 1] < d[tau]) ++tau;
                best = tau;
                break;
            }
        }
        if (best == 0) return;  // not periodic enough

        // Parabolic interpolation for sub-sample accuracy.
        float tau = static_cast<float>(best);
        if (best > 1 && best + 1 <= kMaxTau) {
            const float a = d[best - 1], b = d[best], c = d[best + 1];
            const float denom = a - 2 * b + c;
            if (std::fabs(denom) > 1e-9f) tau += 0.5f * (a - c) / denom;
        }
        const float f0 = kRate / tau;
        if (f0 < kMinF0 || f0 > kMaxF0) return;
        current_ = f0;

        // Median of the last few voiced frames rejects the odd octave jump.
        hist_[histCount_ % kHist] = std::log2(f0);
        ++histCount_;
        std::array<float, kHist> sorted = hist_;
        const int have = std::min(histCount_, kHist);
        std::sort(sorted.begin(), sorted.begin() + have);
        const float median = sorted[have / 2];

        // Slow average: quick at first, then about a second of speech.
        ++voicedCount_;
        if (hasAverage_ && std::fabs(median - avgLog2_) > 0.7f) return;  // outlier relative to what we know
        const float alpha = std::max(0.01f, 1.0f / std::min(voicedCount_, 100));
        avgLog2_ += (median - avgLog2_) * (hasAverage_ ? alpha : 1.0f);
        if (voicedCount_ >= 6) hasAverage_ = true;
    }

    static constexpr int kHist = 9;
    float buf_[kBuf] = {};
    int fill_ = 0;
    std::array<float, kHist> hist_{};
    int histCount_ = 0, voicedCount_ = 0;
    float avgLog2_ = 6.97f;
    float current_ = 0;
    bool hasAverage_ = false;
};
