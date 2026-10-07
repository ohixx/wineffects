#include <climits>
#include <cstring>

#include "effect.h"
#include "rnnoise.h"
#include "signalsmith-stretch/signalsmith-stretch.h"

namespace {

// RNNoise: recurrent neural network trained to remove background noise from speech.
class NoiseSuppression : public Effect {
public:
    NoiseSuppression() {
        params.emplace_back("strength", "Strength", 0, 100, 100, "%d%%");
        state_ = rnnoise_create(nullptr);
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

        float in[kBlockSize], out[kBlockSize];
        for (int i = 0; i < kBlockSize; ++i) in[i] = s[i] * 32768.0f;  // RNNoise expects 16-bit range
        rnnoise_process_frame(state_, out, in);
        for (int i = 0; i < kBlockSize; ++i) s[i] = out[i] * (1.0f / 32768.0f) * wet + s[i] * (1.0f - wet);
    }

    int latencySamples() const override { return 0; }

private:
    DenoiseState* state_ = nullptr;
};

// Signalsmith Stretch: phase-vocoder pitch shifter that keeps the speech speed.
class Pitch : public Effect {
public:
    Pitch() {
        params.emplace_back("semitones", "Shift", -12, 12, 0, "%+d st", true);
        configure(kSampleRate);
    }

    const char* id() const override { return "pitch"; }
    const char* title() const override { return "Pitch"; }
    const char* description() const override { return "Shifts the voice up or down in semitones"; }

    void prepare(int sampleRate) override {
        configure(sampleRate);
        active_ = false;
    }

    void process(float* s, int count) override {
        const int semis = params[0].value.load(std::memory_order_relaxed);
        if (semis == 0) {
            active_ = false;  // pass through untouched, no added delay
            return;
        }
        if (!active_) {
            stretch_.reset();
            lastSemis_ = INT_MIN;
            active_ = true;
        }
        if (semis != lastSemis_) {
            stretch_.setTransposeSemitones(static_cast<float>(semis));
            lastSemis_ = semis;
        }

        float tmp[kBlockSize];
        float* in[1] = {s};
        float* out[1] = {tmp};
        stretch_.process(in, count, out, count);
        std::memcpy(s, tmp, sizeof(float) * count);
    }

    int latencySamples() const override {
        return params[0].value.load(std::memory_order_relaxed) != 0 ? latency_ : 0;
    }

private:
    void configure(int sampleRate) {
        stretch_.configure(1, static_cast<int>(sampleRate * 0.075), static_cast<int>(sampleRate * 0.02));
        stretch_.reset();
        latency_ = stretch_.inputLatency() + stretch_.outputLatency();
    }

    signalsmith::stretch::SignalsmithStretch<float> stretch_;
    bool active_ = false;
    int lastSemis_ = INT_MIN;
    int latency_ = 0;
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
