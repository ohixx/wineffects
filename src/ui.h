#pragma once

#include "app.h"

// scale: DPI scale factor (1.0 = 96 dpi). Call once after the ImGui context is created.
void ApplyTheme(float scale);

// Draws the whole window. Call once per frame between NewFrame() and Render().
void DrawUI(App& app);
