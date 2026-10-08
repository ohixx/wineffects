#include <climits>
#include <cmath>
#include <cstring>

#include "effect.h"
#include "pitch_shifter.h"
#include "rnnoise.h"
#include "signalsmith-stretch/signalsmith-stretch.h"

extern "C" {
extern const unsigned char rnnoise_blob_start[];
extern const unsigned char rnnoise_blob_end[];
}

namespace {

// RNNoise 0.2: recurrent neural network trained to remove background noise from speech.
class NoiseSuppression : public Effect {
public:
    NoiseSuppression() {
        params.emplace_back("strength", "Strength", 0, 100, 100, "%d%%");
        // Mutes the output whenever RNNoise thinks nobody is speaking (like EasyEffects' VAD threshold).
        params.emplace_back("gate", "Voice gate", 0, 100, 0, "%d%%");

        static RNNModel* model = rnnoise_model_from_buffer(
            rnnoise_blob_start, static_cast<int>(rnnoise_blob_end - rnnoise_blob_start));
        state_ = rnnoise_create(model);
    }
    ~NoiseSuppression() override {
        if (state_) rnnoise_destroy(state_);
    }

    const char* id() const override { return "noise"; }
    const char* title() const override { return "Noise suppression"; }
    const char* description() const override { return "Removes keyboard, fans and room noise (RNNoise)"; }

    void process(float* s, int count) override {
        if (!state_ || count != kBlockSize) return;
        const float wet = params[0].value.load(std::memory_order_relaxed) / 100.0f;
        const float threshold = params[1].value.load(std::memory_order_relaxed) / 100.0f;

        float in[kBlockSize], out[kBlockSize];
        for (int i = 0; i < kBlockSize; ++i) in[i] = s[i] * 32768.0f;  // RNNoise expects 16-bit range
        const float vad = rnnoise_process_frame(state_, out, in);

        if (threshold > 0.0f) {
            if (vad >= threshold)
                hold_ = kHoldBlocks;
            else if (hold_ > 0)
                --hold_;
        } else {
            hold_ = kHoldBlocks;  // gate off
        }
        const float target = hold_ > 0 ? 1.0f : 0.0f;

        for (int i = 0; i < kBlockSize; ++i) {
            // The network delays its output by two frames, so the dry signal has to wait as well.
            const float dry = delay_[delayPos_];
            delay_[delayPos_] = s[i];
            delayPos_ = (delayPos_ + 1) % kDelay;

            gain_ += (target - gain_) * (target > gain_ ? 0.02f : 0.0007f);  // ~1 ms attack, ~30 ms release
            s[i] = (out[i] * (1.0f / 32768.0f) * wet + dry * (1.0f - wet)) * gain_;
        }
    }

    int latencySamples() const override { return kDelay; }

private:
    static constexpr int kDelay = 2 * kBlockSize;  // measured: 960 samples
    static constexpr int kHoldBlocks = 15;         // keep the gate open for 150 ms after speech

    DenoiseState* state_ = nullptr;
    float delay_[kDelay] = {};
    int delayPos_ = 0;
    int hold_ = kHoldBlocks;
    float gain_ = 1.0f;
};

// Two ways to shift the pitch:
//  Natural: time-domain stretch + resampling, the same approach as SoundTouch (used by EasyEffects).
//  Smooth:  phase vocoder (Signalsmith Stretch) that also keeps your voice timbre.
class Pitch : public Effect {
public:
    Pitch() {
        params.emplace_back("semitones", "Shift", -12, 12, 0, "%+d st", true);
        params.emplace_back("method", "Method", 0, 1, 0, "%d").choices = {"Natural", "Smooth"};
        shifter_.configure(kSampleRate);
        configureStretch(kSampleRate);
    }

    const char* id() const override { return "pitch"; }
    const char* title() const override { return "Pitch"; }
    const char* description() const override { return "Shifts the voice up or down in semitones"; }

    void prepare(int sampleRate) override {
        shifter_.configure(sampleRate);
        configureStretch(sampleRate);
        active_ = false;
    }

    void process(float* s, int count) override {
        const int semis = params[0].value.load(std::memory_order_relaxed);
        const int method = params[1].value.load(std::memory_order_relaxed);
        if (semis == 0) {
            active_ = false;  // pass through untouched, no added delay
            return;
        }
        if (!active_ || method != lastMethod_) {
            shifter_.reset();
            stretch_.reset();
            lastSemis_ = INT_MIN;
            lastMethod_ = method;
            active_ = true;
        }
        if (semis != lastSemis_) {
            shifter_.setRatio(std::pow(2.0f, semis / 12.0f));
            stretch_.setTransposeSemitones(static_cast<float>(semis));
            stretch_.setFormantSemitones(-static_cast<float>(semis));  // keep the timbre
            lastSemis_ = semis;
        }

        float tmp[kBlockSize];
        if (method == 0) {
            shifter_.process(s, tmp, count);
        } else {
            float* in[1] = {s};
            float* out[1] = {tmp};
            stretch_.process(in, count, out, count);
        }
        std::memcpy(s, tmp, sizeof(float) * count);
    }

    int latencySamples() const override {
        if (params[0].value.load(std::memory_order_relaxed) == 0) return 0;
        return params[1].value.load(std::memory_order_relaxed) == 0 ? shifter_.latency() : stretchLatency_;
    }

private:
    void configureStretch(int sampleRate) {
        stretch_.configure(1, static_cast<int>(sampleRate * 0.075), static_cast<int>(sampleRate * 0.02));
        stretch_.reset();
        stretchLatency_ = stretch_.inputLatency() + stretch_.outputLatency();
    }

    PitchShifter shifter_;
    signalsmith::stretch::SignalsmithStretch<float> stretch_;
    bool active_ = false;
    int lastSemis_ = INT_MIN;
    int lastMethod_ = -1;
    int stretchLatency_ = 0;
};

template <typename T>
std::unique_ptr<Effect> Make() {
    return std::make_unique<T>();
}

}  // namespace

const std::vector<EffectInfo>& AvailableEffects() {
    static const std::vector<EffectInfo> list = {
        {"noise", "Noise suppression", "Removes keyboard, fans and room noise (RNNoise)", &Make<NoiseSuppression>},
        {"pitch", "Pitch", "Shifts the voice up or down in semitones", &Make<Pitch>},
    };
    return list;
}

std::unique_ptr<Effect> CreateEffect(const std::string& id) {
    for (const EffectInfo& e : AvailableEffects())
        if (id == e.id) return e.create();
    return nullptr;
}
