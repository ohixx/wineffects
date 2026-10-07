#pragma once

#include <memory>
#include <string>
#include <vector>

#include "effect.h"

struct EngineConfig {
    std::string input;    // capture device name, empty = system default
    std::string output;   // playback device name (the virtual cable), empty = system default
    std::string monitor;  // headphones device name, empty = system default
    bool monitorEnabled = false;
};

// Microphone -> effect chain -> output device (and optionally a monitor device).
class Engine {
public:
    Engine();
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    void refreshDevices();
    const std::vector<std::string>& inputDevices() const;
    const std::vector<std::string>& outputDevices() const;

    // Returns an empty string on success, otherwise a human-readable error.
    std::string start(const EngineConfig& config);
    void stop();
    bool running() const;

    // Effect chain. Safe to modify while running.
    std::vector<std::shared_ptr<Effect>> chain() const;
    void setChain(std::vector<std::shared_ptr<Effect>> chain);
    void addEffect(const std::string& id);
    void removeEffect(size_t index);
    void moveEffect(size_t index, int direction);

    // 0..1 peak levels for the meters.
    float inputLevel() const;
    float outputLevel() const;

    // Rough end-to-end delay estimate in milliseconds.
    int latencyMs() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
