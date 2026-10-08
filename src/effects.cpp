#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "effect.h"
#include "pitch_shifter.h"
#include "pitch_tracker.h"
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
        stretch_.configure(1, static_cast<int>(sampleRate * 0.065), static_cast<int>(sampleRate * 0.017));
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


// Second-order low-pass used by the character effects.
struct Lowpass {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void set(float sampleRate, float cutoff, float q = 0.7071f) {
        cutoff = std::min(cutoff, sampleRate * 0.45f);
        const float w = 2.0f * 3.14159265f * cutoff / sampleRate;
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

// Trash: the sound of a blown-out cheap microphone. Loud, noisy and muffled; for beefs and toxic battles.
//   high-pass -> drive -> clip -> bit crush -> noise and crackle -> muffle -> loudness -> limiter
class Trash : public Effect {
public:
    Trash() {
        params.emplace_back("drive", "Distortion", 0, 100, 55, "%d%%");
        params.emplace_back("muffle", "Muffle", 0, 100, 60, "%d%%");
        params.emplace_back("crush", "Bit crush", 0, 100, 25, "%d%%");
        params.emplace_back("noise", "Noise", 0, 100, 25, "%d%%");
        params.emplace_back("loud", "Loudness", 0, 100, 60, "%d%%");
    }

    const char* id() const override { return "trash"; }
    const char* title() const override { return "Trash"; }
    const char* description() const override { return "Loud, noisy, muffled voice for beefs and toxic battles"; }

    void process(float* s, int count) override {
        const float drive = params[0].value.load(std::memory_order_relaxed) / 100.0f;
        const int muffle = params[1].value.load(std::memory_order_relaxed);
        const float crush = params[2].value.load(std::memory_order_relaxed) / 100.0f;
        const float noise = params[3].value.load(std::memory_order_relaxed) / 100.0f;
        const float loud = params[4].value.load(std::memory_order_relaxed) / 100.0f;

        if (muffle != lastMuffle_) {  // 12 kHz (no muffling) down to 720 Hz (a phone in a pocket)
            const float cutoff = 12000.0f * std::pow(0.06f, muffle / 100.0f);
            lp1_.set(kSampleRate, cutoff);
            lp2_.set(kSampleRate, cutoff, 0.9f);
            lastMuffle_ = muffle;
        }

        const float pre = 1.0f + drive * 30.0f;
        const int hold = 1 + static_cast<int>(crush * 7.0f);                // sample-rate reduction up to 8x
        const float levels = std::exp2(15.0f - crush * 9.0f);                // 16 bit down to 7 bit
        const float hiss = noise * 0.04f;
        const float popChance = noise * 0.000006f;                           // up to ~30 crackles per second
        const float maxGain = 1.0f + loud * 7.0f;

        for (int i = 0; i < count; ++i) {
            // Rumble filter (about 90 Hz), so low thumps do not eat the loudness.
            float x = hpA_ * (hpY_ + s[i] - hpX_);
            hpX_ = s[i];
            hpY_ = x;

            x = std::tanh(x * pre);  // overdrive

            if (holdCount_++ % hold == 0) held_ = std::round(x * levels) / levels;
            x = held_;

            x += hiss * white();
            if (uniform() < popChance) x += (white() > 0 ? 0.7f : -0.7f);

            x = lp2_.tick(lp1_.tick(x));

            // Fast compressor with make-up gain: pushes everything towards one loud level.
            const float a = std::fabs(x);
            env_ += (a - env_) * (a > env_ ? 0.02f : 0.0003f);
            const float want = std::min(maxGain, 0.5f / (env_ + 1e-4f));
            gain_ += (want - gain_) * 0.01f;
            x *= (loud > 0.0f ? gain_ : 1.0f);

            s[i] = std::tanh(x) * 0.98f;
        }
    }

private:
    uint32_t next() {  // xorshift32
        rng_ ^= rng_ << 13;
        rng_ ^= rng_ >> 17;
        rng_ ^= rng_ << 5;
        return rng_;
    }
    float uniform() { return (next() >> 8) * (1.0f / 16777216.0f); }
    float white() { return uniform() * 2.0f - 1.0f; }

    Lowpass lp1_, lp2_;
    int lastMuffle_ = -1;
    const float hpA_ = 0.9882f;  // one-pole high-pass at ~90 Hz for 48 kHz
    float hpX_ = 0, hpY_ = 0;
    int holdCount_ = 0;
    float held_ = 0;
    float env_ = 0, gain_ = 1.0f;
    uint32_t rng_ = 2463534242u;
};

// RBJ filters used to colour the Female voice.
struct Highpass {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void set(float sampleRate, float cutoff, float q = 0.7071f) {
        const float w = 2.0f * 3.14159265f * cutoff / sampleRate;
        const float alpha = std::sin(w) / (2.0f * q), c = std::cos(w), a0 = 1.0f + alpha;
        b0 = (1.0f + c) * 0.5f / a0;
        b1 = -(1.0f + c) / a0;
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

struct HighShelf {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void set(float sampleRate, float freq, float gainDb) {
        const float A = std::pow(10.0f, gainDb / 40.0f);
        const float w = 2.0f * 3.14159265f * freq / sampleRate;
        const float c = std::cos(w), sn = std::sin(w);
        const float alpha = sn / 2.0f * std::sqrt(2.0f);  // shelf slope 1
        const float beta = 2.0f * std::sqrt(A) * alpha;
        const float a0 = (A + 1) - (A - 1) * c + beta;
        b0 = A * ((A + 1) + (A - 1) * c + beta) / a0;
        b1 = -2 * A * ((A - 1) + (A + 1) * c) / a0;
        b2 = A * ((A + 1) + (A - 1) * c - beta) / a0;
        a1 = 2 * ((A - 1) - (A + 1) * c) / a0;
        a2 = ((A + 1) - (A - 1) * c - beta) / a0;
    }
    float tick(float x) {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// Female: turns any voice into a female one while keeping how you speak.
//
//  * The speaker's average pitch is tracked and the voice is shifted by ONE slowly changing ratio, so
//    the melody of the speech survives (forcing a fixed pitch would sound like a robot).
//  * The shift is limited ("Max shift"): pushing a deep voice a full octave and a half up is what makes
//    a chipmunk. A deep voice ends up as a lower female voice instead, which is how real ones sound.
//  * Gender is also carried by the vocal tract, so the formants move with the shift (ratio^0.3:
//    about +17% for a typical man, +30% for a child, nothing for a voice that is already high).
//  * Breath and brightness add the airy, bright colour women's voices have and hide shifting artefacts.
class Female : public Effect {
public:
    Female() {
        params.emplace_back("age", "Age", 10, 70, 22, "%d yrs");
        params.emplace_back("amount", "Amount", 0, 100, 100, "%d%%");
        params.emplace_back("limit", "Max shift", 0, 16, 10, "%d st");
        params.emplace_back("breath", "Breath", 0, 100, 30, "%d%%");
        params.emplace_back("bright", "Brightness", 0, 100, 40, "%d%%");
        configure(kSampleRate);
        tracker_.reset();
    }

    const char* id() const override { return "female"; }
    const char* title() const override { return "Female"; }
    const char* description() const override { return "Makes any voice female; choose how old she sounds"; }

    void prepare(int sampleRate) override {
        configure(sampleRate);
        tracker_.reset();
        first_ = true;
    }

    void process(float* s, int count) override {
        if (count != kBlockSize) return;
        tracker_.push(s, count);

        const int age = params[0].value.load(std::memory_order_relaxed);
        const float amount = params[1].value.load(std::memory_order_relaxed) / 100.0f;
        const float cap = params[2].value.load(std::memory_order_relaxed) / 12.0f;  // octaves
        const float breath = params[3].value.load(std::memory_order_relaxed) / 100.0f;
        const int bright = params[4].value.load(std::memory_order_relaxed);

        // How far to move: towards an adult woman's pitch, but never beyond the cap; age adds on top.
        const float f0 = tracker_.averageF0();
        const float adult = targetF0(22);
        float wanted = std::min(std::log2(adult / f0), cap) + 0.8f * std::log2(targetF0(age) / adult);
        wanted = std::clamp(wanted, -0.5f, 1.6f) * amount;
        if (first_) {
            smooth_ = wanted;
            first_ = false;
        }
        smooth_ += (wanted - smooth_) * 0.05f;  // ~200 ms: follows the voice, ignores single syllables

        if (std::fabs(smooth_ - applied_) > 0.002f) {
            const float ratio = std::exp2(smooth_);
            const float formants = std::exp2(smooth_ * 0.30f);
            stretch_.setTransposeFactor(ratio);
            stretch_.setFormantFactor(formants / ratio);  // overall formant shift = ratio * this = formants
            applied_ = smooth_;
        }
        if (bright != lastBright_) {
            shelf_.set(kSampleRate, 3200.0f, 5.0f * bright / 100.0f);
            lastBright_ = bright;
        }

        float tmp[kBlockSize];
        float* in[1] = {s};
        float* out[1] = {tmp};
        stretch_.process(in, count, out, count);

        const float breathGain = breath * 0.36f;
        for (int i = 0; i < count; ++i) {
            float y = lowCut_.tick(tmp[i]);

            // Breath: band-limited noise that follows the loudness of the voice, so it is silent in pauses.
            const float a = std::fabs(y);
            env_ += (a - env_) * (a > env_ ? 0.01f : 0.0008f);
            y += noiseLp_.tick(noiseHp_.tick(white())) * env_ * breathGain;

            s[i] = shelf_.tick(y);
        }
    }

    int latencySamples() const override { return latency_; }

private:
    // Typical average speaking pitch of women by age.
    static float targetF0(int age) {
        static const int ages[] = {10, 14, 18, 25, 35, 45, 55, 70};
        static const float f0[] = {300, 262, 236, 216, 208, 198, 186, 172};
        age = std::clamp(age, ages[0], ages[7]);
        for (int i = 0; i < 7; ++i)
            if (age <= ages[i + 1]) return f0[i] + (f0[i + 1] - f0[i]) * (age - ages[i]) / (ages[i + 1] - ages[i]);
        return f0[7];
    }

    float white() {  // xorshift32
        rng_ ^= rng_ << 13;
        rng_ ^= rng_ >> 17;
        rng_ ^= rng_ << 5;
        return (rng_ >> 8) * (1.0f / 8388608.0f) - 1.0f;
    }

    void configure(int sampleRate) {
        stretch_.configure(1, static_cast<int>(sampleRate * 0.065), static_cast<int>(sampleRate * 0.017));
        stretch_.reset();
        latency_ = stretch_.inputLatency() + stretch_.outputLatency();
        applied_ = 1000.0f;  // force the first update
        lowCut_.set(static_cast<float>(sampleRate), 90.0f);
        noiseHp_.set(static_cast<float>(sampleRate), 2500.0f);
        noiseLp_.set(static_cast<float>(sampleRate), 8000.0f);
        lastBright_ = -1;
    }

    PitchTracker tracker_;
    signalsmith::stretch::SignalsmithStretch<float> stretch_;
    Highpass lowCut_, noiseHp_;
    Lowpass noiseLp_;
    HighShelf shelf_;
    float smooth_ = 0.0f, applied_ = 1000.0f, env_ = 0.0f;
    bool first_ = true;
    int latency_ = 0, lastBright_ = -1;
    uint32_t rng_ = 88172645u;
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
        {"female", "Female", "Makes any voice female; choose how old she sounds", &Make<Female>},
        {"trash", "Trash", "Loud, noisy, muffled voice for beefs and toxic battles", &Make<Trash>},
    };
    return list;
}

std::unique_ptr<Effect> CreateEffect(const std::string& id) {
    for (const EffectInfo& e : AvailableEffects())
        if (id == e.id) return e.create();
    return nullptr;
}
