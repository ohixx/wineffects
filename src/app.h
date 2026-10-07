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
    bool autoStartEngine = false;  // start processing on launch
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

    void start();
    void stop();
    void toggle();
    void restartIfRunning();
};
