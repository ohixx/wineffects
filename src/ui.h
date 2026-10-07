#pragma once

// Everything the interface edits. The audio engine will read these later.
struct AppState {
    bool running = false;

    int inputDevice = 0;
    int outputDevice = 0;
    char micName[64] = "WinEffects Microphone";

    bool noiseEnabled = true;
    int noiseStrength = 80;  // percent

    bool pitchEnabled = true;
    int pitch = 0;  // semitones, -12..+12
};

// scale: DPI scale factor (1.0 = 96 dpi). Call once after the ImGui context is created.
void ApplyTheme(float scale);

// Draws the whole window. Call once per frame between NewFrame() and Render().
void DrawUI(AppState& state);
