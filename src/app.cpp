#include "app.h"

#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "platform.h"

namespace {

namespace fs = std::filesystem;

fs::path SettingsPath() {
    const wchar_t* appdata = _wgetenv(L"APPDATA");
    fs::path dir = appdata ? fs::path(appdata) / L"WinEffects" : fs::path(L".");
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir / L"settings.ini";
}

std::string Trim(std::string s) {
    const char* ws = " \t\r\n";
    const size_t a = s.find_first_not_of(ws);
    if (a == std::string::npos) return {};
    return s.substr(a, s.find_last_not_of(ws) - a + 1);
}

bool Bool(const std::string& v) { return v == "1" || v == "true"; }

}  // namespace

void App::load() {
    engine.refreshDevices();

    std::ifstream in(SettingsPath(), std::ios::binary);
    const bool firstRun = !in.good();

    std::shared_ptr<Effect> current;
    std::vector<std::shared_ptr<Effect>> chain;
    bool inEffect = false;
    bool hadChain = false;

    std::string line;
    while (in && std::getline(in, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#') continue;
        if (line == "[effect]") {
            inEffect = true;
            hadChain = true;
            current.reset();
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = Trim(line.substr(0, eq));
        const std::string val = Trim(line.substr(eq + 1));

        if (inEffect) {
            if (key == "id") {
                current = CreateEffect(val);
                if (current) chain.push_back(current);
            } else if (current && key == "enabled") {
                current->enabled = Bool(val);
            } else if (current) {
                for (Param& p : current->params) {
                    if (p.key == key) {
                        try {
                            p.value = std::clamp(std::stoi(val), p.min, p.max);
                        } catch (...) {
                        }
                    }
                }
            }
            continue;
        }

        if (key == "input") settings.input = val;
        else if (key == "output") settings.output = val;
        else if (key == "monitor") settings.monitor = val;
        else if (key == "monitor_enabled") settings.monitorEnabled = Bool(val);
        else if (key == "mic_name") settings.micName = val;
        else if (key == "close_to_tray") settings.closeToTray = Bool(val);
        else if (key == "start_minimized") settings.startMinimized = Bool(val);
        else if (key == "low_latency_devices") settings.lowLatencyDevices = Bool(val);
        else if (key == "buffer_ms") {
            try {
                settings.bufferMs = std::clamp(std::stoi(val), 2, 100);
                if (settings.bufferMs == 15 || settings.bufferMs == 20) settings.bufferMs = 5;  // old defaults
            } catch (...) {
            }
        }
    }

    settings.autostart = IsAutostartEnabled();

    selectVirtualCable();
    if (firstRun)
        engine.addEffect("noise");
    else if (hadChain)
        engine.setChain(std::move(chain));
}

std::string App::virtualCableName() const {
    for (const std::string& name : engine.outputDevices())
        if (name.find("CABLE Input") != std::string::npos) return name;
    return {};
}

// If no output has been chosen yet, prefer an installed virtual cable over the speakers.
void App::selectVirtualCable() {
    const std::string cable = virtualCableName();
    if (settings.output.empty() && !cable.empty()) {
        settings.output = cable;
        dirty = true;
    }
}

void App::save() {
    std::ofstream out(SettingsPath(), std::ios::binary | std::ios::trunc);
    if (!out) return;

    out << "input=" << settings.input << "\n";
    out << "output=" << settings.output << "\n";
    out << "monitor=" << settings.monitor << "\n";
    out << "monitor_enabled=" << settings.monitorEnabled << "\n";
    out << "mic_name=" << settings.micName << "\n";
    out << "close_to_tray=" << settings.closeToTray << "\n";
    out << "start_minimized=" << settings.startMinimized << "\n";
    out << "buffer_ms=" << settings.bufferMs << "\n";
    out << "low_latency_devices=" << settings.lowLatencyDevices << "\n";

    for (const auto& e : engine.chain()) {
        out << "\n[effect]\n";
        out << "id=" << e->id() << "\n";
        out << "enabled=" << e->enabled.load() << "\n";
        for (const Param& p : e->params) out << p.key << "=" << p.value.load() << "\n";
    }
    dirty = false;
}

void App::start() {
    EngineConfig cfg;
    cfg.input = settings.input;
    cfg.output = settings.output;
    cfg.monitor = settings.monitor;
    cfg.monitorEnabled = settings.monitorEnabled || soundCheck;
    cfg.bufferMs = settings.bufferMs;
    cfg.lowLatencyDevices = settings.lowLatencyDevices;
    error = engine.start(cfg);
}

void App::stop() { engine.stop(); }

void App::toggle() {
    paused = !paused;
    if (paused) {
        soundCheck = false;
        stop();
        error.clear();
    } else {
        start();
    }
}

void App::setSoundCheck(bool on) {
    if (on && paused) return;
    soundCheck = on;
    soundCheckEndMs = GetTickCount64() + 30000;  // never leave it on by accident
    restartIfRunning();
}

void App::tick() {
    const ULONGLONG now = GetTickCount64();
    if (soundCheck && now >= soundCheckEndMs) setSoundCheck(false);

    // Keep going: retry every few seconds if the devices were not available (or went away).
    static ULONGLONG lastTry = 0;
    if (!paused && !engine.running() && now - lastTry > 3000) {
        lastTry = now;
        engine.refreshDevices();
        selectVirtualCable();
        start();
    }
}

void App::restartIfRunning() {
    if (engine.running()) start();
}
