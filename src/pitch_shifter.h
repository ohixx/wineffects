#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

// Real-time pitch shifter for mono speech, built the way SoundTouch (and so EasyEffects) does it:
//
//   input -> time-domain stretch (SOLA: overlap-add at the best waveform match) -> resampler
//
// To raise the pitch by a ratio r the signal is first stretched to r times its length without
// changing its pitch, then read back r times faster. Working on the waveform instead of an FFT
// keeps speech sounding natural; the price is that formants move with the pitch.
//
// All buffers are allocated in configure(), so process() is safe to call from the audio thread.
class PitchShifter {
public:
    void configure(int sampleRate, int sequenceMs = 24, int overlapMs = 5, int seekMs = 12) {
        sampleRate_ = sampleRate;
        sequence_ = sampleRate * sequenceMs / 1000;  // segments
        overlap_ = sampleRate * overlapMs / 1000;    // cross-fade
        seek_ = sampleRate * seekMs / 1000;          // search window (should cover a period of a low voice)
        const size_t cap = 1 << 15;
        in_.init(cap);
        stretched_.init(cap);
        out_.init(cap);
        mid_.assign(overlap_, 0.0f);
        window_.resize(overlap_);
        for (int i = 0; i < overlap_; ++i) window_[i] = (i + 0.5f) / overlap_;
        reset();
        setRatio(1.0f);
    }

    void reset() {
        in_.clear();
        stretched_.clear();
        out_.clear();
        const float zero = 0.0f;
        stretched_.push(&zero, 1);  // the resampler looks one sample behind its read position
        std::fill(mid_.begin(), mid_.end(), 0.0f);
        haveMid_ = false;
        skipFract_ = 0.0;
        pos_ = 1.0;
        primed_ = false;
        if (primeOverride_ < 0) prime_ = kBlock;
        lp_[0] = lp_[1] = Biquad();
    }

    // Pitch multiplier, 0.5 .. 2.0.
    void setRatio(float ratio) {
        ratio_ = std::clamp(ratio, 0.5f, 2.0f);
        // Anti-alias filter in front of the resampler when it reads faster than real time.
        if (ratio_ > 1.0f) {
            const float fc = 0.46f * sampleRate_ / ratio_;
            lp_[0].lowpass(sampleRate_, fc, 0.54f);
            lp_[1].lowpass(sampleRate_, fc, 1.31f);
            filter_ = true;
        } else {
            filter_ = false;
        }
    }

    // Consumes n samples and produces n samples. The output is silent until enough has been
    // buffered (see latency()).
    void process(const float* in, float* out, int n) {
        in_.push(in, n);
        while (stretchStep()) {
        }
        resample();

        if (!primed_ && out_.size() >= static_cast<size_t>(prime_)) primed_ = true;
        if (!primed_) {
            std::memset(out, 0, sizeof(float) * n);
            return;
        }
        const size_t have = std::min<size_t>(out_.size(), n);
        std::memcpy(out, out_.data(), sizeof(float) * have);
        out_.consume(have);
        if (have < static_cast<size_t>(n)) {  // underrun: should not happen in steady state
            std::memset(out + have, 0, sizeof(float) * (n - have));
            primed_ = false;
            ++underruns_;
        }
    }

    // Approximate delay in samples (measured on bursts): input a stretch step must collect, plus the
    // first finished burst it delivers.
    int latency() const { return sequence_ + seek_ - kBlock + static_cast<int>((sequence_ - overlap_) / ratio_ / 2); }
    void forcePrime(int samples) { primeOverride_ = samples; prime_ = samples; }  // for tests
    unsigned underruns() const { return underruns_; }

private:
    struct Fifo {
        std::vector<float> buf;
        size_t head = 0, tail = 0;
        void init(size_t cap) {
            buf.assign(cap, 0.0f);
            head = tail = 0;
        }
        void clear() { head = tail = 0; }
        size_t size() const { return tail - head; }
        const float* data() const { return buf.data() + head; }
        float* writePtr(size_t n) {
            if (tail + n > buf.size()) {  // compact
                std::memmove(buf.data(), buf.data() + head, sizeof(float) * (tail - head));
                tail -= head;
                head = 0;
            }
            return tail + n <= buf.size() ? buf.data() + tail : nullptr;
        }
        void push(const float* p, size_t n) {
            float* w = writePtr(n);
            if (!w) return;  // overflow: drop (never happens with sane block sizes)
            std::memcpy(w, p, sizeof(float) * n);
            tail += n;
        }
        void consume(size_t n) { head += std::min(n, size()); }
    };

    struct Biquad {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void lowpass(float sr, float fc, float q) {
            const float w = 2.0f * 3.14159265f * fc / sr;
            const float alpha = std::sin(w) / (2.0f * q);
            const float c = std::cos(w);
            const float a0 = 1.0f + alpha;
            b0 = (1.0f - c) * 0.5f / a0;
            b1 = (1.0f - c) / a0;
            b2 = b0;
            a1 = -2.0f * c / a0;
            a2 = (1.0f - alpha) / a0;
        }
        float tick(float x) {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    // One iteration of the time-domain stretch. Returns false when it needs more input.
    bool stretchStep() {
        const int need = seek_ + sequence_;
        if (in_.size() < static_cast<size_t>(need)) return false;
        const float* x = in_.data();

        // Where does the next segment fit best onto the tail of the previous one?
        int best = 0;
        if (haveMid_) {
            double bestScore = -1e30;
            double energy = 0;
            for (int i = 0; i < overlap_; ++i) energy += double(x[i]) * x[i];
            for (int o = 0; o < seek_; ++o) {
                double dot = 0;
                const float* s = x + o;
                for (int i = 0; i < overlap_; ++i) dot += double(mid_[i]) * s[i];
                const double score = dot / std::sqrt(energy + 1e-3);  // normalised, so loud spots don't win
                if (score > bestScore) {
                    bestScore = score;
                    best = o;
                }
                energy += double(s[overlap_]) * s[overlap_] - double(s[0]) * s[0];
            }
        }
        const float* seg = x + best;

        float chunk[8192];
        const int n = sequence_ - overlap_;
        if (haveMid_) {
            for (int i = 0; i < overlap_; ++i) chunk[i] = mid_[i] * (1.0f - window_[i]) + seg[i] * window_[i];
        } else {
            std::memcpy(chunk, seg, sizeof(float) * overlap_);
        }
        std::memcpy(chunk + overlap_, seg + overlap_, sizeof(float) * (sequence_ - 2 * overlap_));
        std::memcpy(mid_.data(), seg + sequence_ - overlap_, sizeof(float) * overlap_);
        haveMid_ = true;

        if (filter_)
            for (int i = 0; i < n; ++i) chunk[i] = lp_[1].tick(lp_[0].tick(chunk[i]));
        stretched_.push(chunk, n);

        // The next segment starts `tempo * n` samples further on; tempo = 1 / ratio.
        skipFract_ += static_cast<double>(n) / ratio_;
        const size_t adv = static_cast<size_t>(skipFract_);
        skipFract_ -= adv;
        in_.consume(adv);
        return true;
    }

    // Reads the stretched signal `ratio` times faster using cubic (Hermite) interpolation.
    void resample() {
        const float* x = stretched_.data();
        const size_t size = stretched_.size();
        while (pos_ + 2.0 < static_cast<double>(size)) {
            const size_t i = static_cast<size_t>(pos_);
            const float t = static_cast<float>(pos_ - i);
            const float xm1 = x[i - 1], x0 = x[i], x1 = x[i + 1], x2 = x[i + 2];
            const float c1 = 0.5f * (x1 - xm1);
            const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
            const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
            const float y = ((c3 * t + c2) * t + c1) * t + x0;
            out_.push(&y, 1);
            pos_ += ratio_;
        }
        const size_t drop = pos_ >= 1.0 ? static_cast<size_t>(pos_) - 1 : 0;
        stretched_.consume(drop);
        pos_ -= drop;
    }

    static constexpr int kBlock = 480;
    int sampleRate_ = 48000;
    int sequence_ = 1440, overlap_ = 288, seek_ = 576, prime_ = kBlock, primeOverride_ = -1;
    float ratio_ = 1.0f;
    bool filter_ = false;
    Biquad lp_[2];

    Fifo in_, stretched_, out_;
    std::vector<float> mid_, window_;
    bool haveMid_ = false;
    double skipFract_ = 0.0, pos_ = 1.0;
    bool primed_ = false;
    unsigned underruns_ = 0;
};
