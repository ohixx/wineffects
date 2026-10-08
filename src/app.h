#pragma once

#include <string>

#include "engine.h"

struct Settings {
    std::string input;    // microphone, empty = system default
    std::string output;   // virtual cable input, empty = system default
    std::string monitor;  // headphones
    bool monitorEnabled = false;
    std::string micName = "WinEffects Microphone";

    bool autostart = false;       // start with Windows
    bool closeToTray = true;      // closing the window hides it
    bool startMinimized = false;  // start hidden in the tray
    int bufferMs = 20;             // playback buffer
};

// Everything the window and the tray share.
struct App {
    Engine engine;
    Settings settings;
    std::string error;  // last start error, shown in the UI
    bool dirty = false;  // settings changed and need saving

    // Loads settings.ini (and the effect chain). Picks sensible devices on first run.
    void load();
    void save();

    std::string virtualCableName() const;  // "" if none installed
    bool hasVirtualCable() const { return !virtualCableName().empty(); }
    void selectVirtualCable();

    // Processing runs for as long as the app does. Only the tray menu can pause it.
    bool paused = false;
    bool soundCheck = false;  // temporarily play the processed sound to the headphones
    unsigned long long soundCheckEndMs = 0;

    void start();
    void stop();
    void toggle();  // pause / resume (tray menu)
    void restartIfRunning();
    void setSoundCheck(bool on);
    void tick();  // call regularly: keeps processing alive and ends the sound check
};
