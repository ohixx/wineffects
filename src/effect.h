#pragma once

#include <atomic>
#include <deque>
#include <memory>
#include <string>
#include <vector>

// The audio engine always runs mono at 48 kHz in blocks of 10 ms.
constexpr int kSampleRate = 48000;
constexpr int kBlockSize = 480;

// An integer parameter. The UI builds a slider for it automatically, so a new
// effect needs no UI code.
struct Param {
    Param(const char* key, const char* label, int min, int max, int def, const char* format, bool centered = false)
        : key(key), label(label), format(format), min(min), max(max), def(def), centered(centered), value(def) {}

    std::string key;     // stable name used in the settings file
    std::string label;   // shown in the UI
    std::string format;  // printf format taking one int, e.g. "%d%%" or "%+d st"
    int min, max, def;
    bool centered;  // fill the slider from the middle (for +/- ranges)
    std::vector<std::string> choices;  // if set, the value is an index and these are the names
    std::atomic<int> value;
};

class Effect {
public:
    virtual ~Effect() = default;

    virtual const char* id() const = 0;
    virtual const char* title() const = 0;
    virtual const char* description() const = 0;

    // Called on the UI thread before the effect is used by the audio thread.
    virtual void prepare(int /*sampleRate*/) {}

    // Audio thread. Processes exactly kBlockSize mono samples in place.
    virtual void process(float* samples, int count) = 0;

    // Delay added by this effect, in samples (for the latency readout).
    virtual int latencySamples() const { return 0; }

    std::deque<Param> params;
    std::atomic<bool> enabled{true};
};

struct EffectInfo {
    const char* id;
    const char* title;
    const char* description;
    std::unique_ptr<Effect> (*create)();
};

// Every effect the user can add. To add a new one, implement Effect and list it in effects.cpp.
const std::vector<EffectInfo>& AvailableEffects();
std::unique_ptr<Effect> CreateEffect(const std::string& id);
