#include "engine.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>

#include "miniaudio.h"

namespace {

using Chain = std::vector<std::shared_ptr<Effect>>;

// Single-producer / single-consumer ring buffer of mono samples.
class Ring {
public:
    explicit Ring(size_t capacity = 1 << 16) : buf_(capacity), mask_(capacity - 1) {}

    size_t available() const { return w_.load(std::memory_order_acquire) - r_.load(std::memory_order_acquire); }

    bool write(const float* data, size_t n) {
        const size_t w = w_.load(std::memory_order_relaxed);
        const size_t r = r_.load(std::memory_order_acquire);
        if (buf_.size() - (w - r) < n) return false;  // consumer stalled: drop this block
        for (size_t i = 0; i < n; ++i) buf_[(w + i) & mask_] = data[i];
        w_.store(w + n, std::memory_order_release);
        return true;
    }

    size_t read(float* data, size_t n) {
        const size_t r = r_.load(std::memory_order_relaxed);
        const size_t avail = w_.load(std::memory_order_acquire) - r;
        n = std::min(n, avail);
        for (size_t i = 0; i < n; ++i) data[i] = buf_[(r + i) & mask_];
        r_.store(r + n, std::memory_order_release);
        return n;
    }

    void skip(size_t n) { r_.store(r_.load(std::memory_order_relaxed) + n, std::memory_order_release); }

    void reset() {
        r_.store(0);
        w_.store(0);
    }

private:
    std::vector<float> buf_;
    size_t mask_;
    std::atomic<size_t> r_{0}, w_{0};
};

struct Sink {
    ma_device device;
    Ring ring;
    bool inited = false;
    bool primed = false;
    // Playback starts with `prime` samples of silence as a cushion (the processed audio arrives in 10 ms
    // blocks, so the queue needs some slack). If the queue grows past `maxFill` the two devices are
    // drifting apart, so it is trimmed back to `target`.
    size_t prime = 240, maxFill = kBlockSize * 6, target = kBlockSize * 2;
    size_t pad = 0;  // silent samples still to play
    std::atomic<unsigned>* glitches = nullptr;
    std::atomic<float>* level = nullptr;  // peak of the samples delivered to the device
    unsigned callbacks = 0;               // glitches are not counted while the streams settle (first ~3 s)
};

void PlaybackCallback(ma_device* dev, void* output, const void*, ma_uint32 frames) {
    Sink* sink = static_cast<Sink*>(dev->pUserData);
    float* out = static_cast<float*>(output);
    Ring& ring = sink->ring;

    const bool settled = ++sink->callbacks > 300;
    size_t avail = ring.available();
    if (avail > sink->maxFill) {
        ring.skip(avail - sink->target);
        avail = sink->target;
        if (settled) sink->glitches->fetch_add(1, std::memory_order_relaxed);
    }
    if (!sink->primed) {
        sink->pad = sink->prime;
        sink->primed = true;
    }

    float chunk[512];
    float peak = 0;
    ma_uint32 done = 0;
    if (sink->pad > 0) {
        const ma_uint32 silent = static_cast<ma_uint32>(std::min<size_t>(sink->pad, frames));
        std::memset(out, 0, sizeof(float) * 2 * silent);
        sink->pad -= silent;
        done = silent;
    }
    while (done < frames) {
        const size_t want = std::min<size_t>(512, frames - done);
        const size_t got = ring.read(chunk, want);
        for (size_t i = 0; i < got; ++i) {
            out[2 * (done + i)] = out[2 * (done + i) + 1] = chunk[i];
            peak = std::max(peak, std::fabs(chunk[i]));
        }
        done += static_cast<ma_uint32>(got);
        if (got < want) break;
    }
    if (done < frames) {  // underrun: output silence and rebuffer
        std::memset(out + 2 * done, 0, sizeof(float) * 2 * (frames - done));
        sink->primed = false;
        if (settled) sink->glitches->fetch_add(1, std::memory_order_relaxed);
    }
    if (sink->level) sink->level->store(std::max(peak, sink->level->load(std::memory_order_relaxed) * 0.85f));
}

float Peak(const float* s, int n) {
    float p = 0;
    for (int i = 0; i < n; ++i) p = std::max(p, std::fabs(s[i]));
    return p;
}

}  // namespace

struct Engine::Impl {
    ma_context context;
    bool contextOk = false;
    std::vector<ma_device_info> captureInfos, playbackInfos;
    std::vector<std::string> inputNames, outputNames;

    ma_device capture;
    bool captureInited = false;
    Sink output, monitor;
    bool monitorOn = false;
    std::atomic<bool> running{false};
    int bufferMs = 5;
    double capturePeriodMs = 10, playbackPeriodMs = 10;  // what the drivers actually gave us

    std::shared_ptr<const Chain> chain = std::make_shared<const Chain>();

    float acc[kBlockSize];
    int fill = 0;
    std::atomic<float> inLevel{0}, outLevel{0};
    std::atomic<unsigned> glitches{0};
    std::atomic<float> deviceLevel{0};
    std::atomic<int> toneBlocks{0};
    double tonePhase = 0;

    void onCapture(const float* in, ma_uint32 n) {
        while (n > 0) {
            const int take = static_cast<int>(std::min<ma_uint32>(n, kBlockSize - fill));
            std::memcpy(acc + fill, in, sizeof(float) * take);
            fill += take;
            in += take;
            n -= take;
            if (fill == kBlockSize) {
                processBlock();
                fill = 0;
            }
        }
    }

    void processBlock() {
        inLevel.store(std::max(Peak(acc, kBlockSize), inLevel.load() * 0.85f));

        std::shared_ptr<const Chain> c = std::atomic_load(&chain);
        for (const auto& e : *c)
            if (e->enabled.load(std::memory_order_relaxed)) e->process(acc, kBlockSize);

        int tone = toneBlocks.load(std::memory_order_relaxed);
        if (tone > 0) {
            for (float& v : acc) {
                v = 0.25f * static_cast<float>(std::sin(tonePhase));
                tonePhase += 2.0 * 3.14159265358979 * 440.0 / kSampleRate;
            }
            toneBlocks.store(tone - 1, std::memory_order_relaxed);
        }

        for (float& v : acc) v = std::clamp(v, -1.0f, 1.0f);
        outLevel.store(std::max(Peak(acc, kBlockSize), outLevel.load() * 0.85f));

        if (!output.ring.write(acc, kBlockSize)) glitches.fetch_add(1, std::memory_order_relaxed);
        if (monitorOn) monitor.ring.write(acc, kBlockSize);
    }

    static void CaptureCallback(ma_device* dev, void*, const void* input, ma_uint32 frames) {
        static_cast<Impl*>(dev->pUserData)->onCapture(static_cast<const float*>(input), frames);
    }

    void closeDevices() {
        if (captureInited) ma_device_uninit(&capture);
        if (output.inited) ma_device_uninit(&output.device);
        if (monitor.inited) ma_device_uninit(&monitor.device);
        captureInited = output.inited = monitor.inited = false;
        monitorOn = false;
    }

    // Initialise a stereo playback device that drains `sink.ring`.
    ma_result openSink(Sink& sink, const ma_device_id* id, int bufferMs, bool lowLatency) {
        const size_t prime = static_cast<size_t>(std::clamp(bufferMs, 2, 200)) * (kSampleRate / 1000);
        sink.prime = prime;
        sink.target = prime + kBlockSize;
        sink.maxFill = prime * 3 + 2 * kBlockSize;
        sink.callbacks = 0;
        sink.glitches = &glitches;
        sink.ring.reset();
        sink.primed = false;
        sink.pad = 0;

        // A period of 1 ms means "as small as the driver allows" (WASAPI's low-latency shared mode);
        // if the driver refuses, fall back to the normal 10 ms.
        ma_result r = MA_ERROR;
        for (int periodMs : {lowLatency ? 1 : 10, 10}) {
            ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
            cfg.playback.pDeviceID = id;
            cfg.playback.format = ma_format_f32;
            cfg.playback.channels = 2;
            cfg.sampleRate = kSampleRate;
            cfg.periodSizeInMilliseconds = periodMs;
            cfg.dataCallback = PlaybackCallback;
            cfg.pUserData = &sink;
            r = ma_device_init(&context, &cfg, &sink.device);
            if (r == MA_SUCCESS || periodMs == 10) break;
        }
        sink.inited = (r == MA_SUCCESS);
        return r;
    }
};

namespace {

// Looks a device up by name. Returns false if a non-empty name is not present.
bool FindDevice(const std::vector<ma_device_info>& infos, const std::string& name, const ma_device_id*& out) {
    out = nullptr;
    if (name.empty()) return true;
    for (const ma_device_info& d : infos) {
        if (name == d.name) {
            out = &d.id;
            return true;
        }
    }
    return false;
}

}  // namespace

Engine::Engine() : impl_(new Impl) {
    impl_->contextOk = ma_context_init(nullptr, 0, nullptr, &impl_->context) == MA_SUCCESS;
    refreshDevices();
}

Engine::~Engine() {
    stop();
    if (impl_->contextOk) ma_context_uninit(&impl_->context);
}

void Engine::refreshDevices() {
    Impl& m = *impl_;
    m.captureInfos.clear();
    m.playbackInfos.clear();
    m.inputNames.clear();
    m.outputNames.clear();
    if (!m.contextOk) return;

    ma_device_info* playback = nullptr;
    ma_device_info* capture = nullptr;
    ma_uint32 playbackCount = 0, captureCount = 0;
    if (ma_context_get_devices(&m.context, &playback, &playbackCount, &capture, &captureCount) != MA_SUCCESS) return;

    for (ma_uint32 i = 0; i < captureCount; ++i) {
        m.captureInfos.push_back(capture[i]);
        m.inputNames.emplace_back(capture[i].name);
    }
    for (ma_uint32 i = 0; i < playbackCount; ++i) {
        m.playbackInfos.push_back(playback[i]);
        m.outputNames.emplace_back(playback[i].name);
    }
}

const std::vector<std::string>& Engine::inputDevices() const { return impl_->inputNames; }
const std::vector<std::string>& Engine::outputDevices() const { return impl_->outputNames; }

std::string Engine::start(const EngineConfig& cfg) {
    stop();
    Impl& m = *impl_;
    if (!m.contextOk) return "Audio system could not be initialised.";
    refreshDevices();

    const ma_device_id *inId = nullptr, *outId = nullptr, *monId = nullptr;
    if (!FindDevice(m.captureInfos, cfg.input, inId)) return "Microphone not found: " + cfg.input;
    if (!FindDevice(m.playbackInfos, cfg.output, outId)) return "Output device not found: " + cfg.output;
    if (cfg.monitorEnabled && !FindDevice(m.playbackInfos, cfg.monitor, monId))
        return "Headphones not found: " + cfg.monitor;

    for (const auto& e : *std::atomic_load(&m.chain)) e->prepare(kSampleRate);
    m.fill = 0;
    m.inLevel = m.outLevel = 0;

    m.glitches = 0;
    m.deviceLevel = 0;
    m.toneBlocks = 0;
    m.output.level = &m.deviceLevel;
    m.monitor.level = nullptr;
    m.bufferMs = cfg.bufferMs;
    ma_result r = m.openSink(m.output, outId, cfg.bufferMs, cfg.lowLatencyDevices);
    if (r != MA_SUCCESS) {
        m.closeDevices();
        return std::string("Cannot open output device: ") + ma_result_description(r);
    }
    if (cfg.monitorEnabled) {
        r = m.openSink(m.monitor, monId, cfg.bufferMs, cfg.lowLatencyDevices);
        if (r != MA_SUCCESS) {
            m.closeDevices();
            return std::string("Cannot open headphones: ") + ma_result_description(r);
        }
        m.monitorOn = true;
    }

    r = MA_ERROR;
    for (int periodMs : {cfg.lowLatencyDevices ? 1 : 10, 10}) {
        ma_device_config cc = ma_device_config_init(ma_device_type_capture);
        cc.capture.pDeviceID = inId;
        cc.capture.format = ma_format_f32;
        cc.capture.channels = 1;
        cc.sampleRate = kSampleRate;
        cc.periodSizeInMilliseconds = periodMs;
        cc.dataCallback = Impl::CaptureCallback;
        cc.pUserData = &m;
        r = ma_device_init(&m.context, &cc, &m.capture);
        if (r == MA_SUCCESS || periodMs == 10) break;
    }
    if (r != MA_SUCCESS) {
        m.closeDevices();
        return std::string("Cannot open microphone: ") + ma_result_description(r);
    }
    m.captureInited = true;

    // Remember the periods that were really granted, for the latency readout.
    if (m.capture.capture.internalSampleRate > 0)
        m.capturePeriodMs = 1000.0 * m.capture.capture.internalPeriodSizeInFrames / m.capture.capture.internalSampleRate;
    if (m.output.device.playback.internalSampleRate > 0)
        m.playbackPeriodMs =
            1000.0 * m.output.device.playback.internalPeriodSizeInFrames / m.output.device.playback.internalSampleRate;

    if ((r = ma_device_start(&m.output.device)) != MA_SUCCESS ||
        (m.monitorOn && (r = ma_device_start(&m.monitor.device)) != MA_SUCCESS) ||
        (r = ma_device_start(&m.capture)) != MA_SUCCESS) {
        m.closeDevices();
        return std::string("Cannot start audio: ") + ma_result_description(r);
    }

    m.running = true;
    return {};
}

void Engine::stop() {
    Impl& m = *impl_;
    if (!m.running && !m.captureInited && !m.output.inited && !m.monitor.inited) return;
    m.closeDevices();
    m.running = false;
    m.inLevel = m.outLevel = 0;
}

bool Engine::running() const { return impl_->running; }

namespace {
void Publish(std::shared_ptr<const Chain>& slot, Chain chain) {
    std::atomic_store(&slot, std::shared_ptr<const Chain>(std::make_shared<const Chain>(std::move(chain))));
}
}  // namespace

std::vector<std::shared_ptr<Effect>> Engine::chain() const { return *std::atomic_load(&impl_->chain); }

void Engine::setChain(std::vector<std::shared_ptr<Effect>> chain) {
    for (auto& e : chain) e->prepare(kSampleRate);
    Publish(impl_->chain, std::move(chain));
}

void Engine::addEffect(const std::string& id) {
    std::unique_ptr<Effect> e = CreateEffect(id);
    if (!e) return;
    e->prepare(kSampleRate);
    Chain c = chain();
    c.push_back(std::move(e));
    Publish(impl_->chain, std::move(c));
}

void Engine::removeEffect(size_t index) {
    Chain c = chain();
    if (index >= c.size()) return;
    c.erase(c.begin() + static_cast<std::ptrdiff_t>(index));
    Publish(impl_->chain, std::move(c));
}

void Engine::moveEffect(size_t index, int direction) {
    Chain c = chain();
    const long target = static_cast<long>(index) + direction;
    if (index >= c.size() || target < 0 || target >= static_cast<long>(c.size())) return;
    std::swap(c[index], c[static_cast<size_t>(target)]);
    Publish(impl_->chain, std::move(c));
}

float Engine::inputLevel() const { return impl_->inLevel.load(); }
float Engine::outputLevel() const { return impl_->outLevel.load(); }

int Engine::latencyMs() const {
    int samples = 0;
    for (const auto& e : chain())
        if (e->enabled.load()) samples += e->latencySamples();
    // A sample waits for its capture period, then (on average) half a 10 ms block, then the cushion in the
    // playback queue (plus half a block of queue slack) and one playback period.
    const double path = impl_->capturePeriodMs + 5.0 + impl_->bufferMs + 5.0 + impl_->playbackPeriodMs;
    return static_cast<int>(samples * 1000.0 / kSampleRate + path + 0.5);
}

float Engine::deviceLevel() const { return impl_->deviceLevel.load(); }

void Engine::playTestTone() {
    if (impl_->running) impl_->toneBlocks.store(200);  // 2 s
}

unsigned Engine::glitches() const { return impl_->glitches.load(); }
