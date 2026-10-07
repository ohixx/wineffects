#include "ui.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>

#include "imgui.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

float g_scale = 1.0f;
ImFont* g_titleFont = nullptr;

float S(float v) { return v * g_scale; }

ImVec4 Hex(unsigned rgb, float a = 1.0f) {
    return ImVec4(((rgb >> 16) & 0xFF) / 255.0f, ((rgb >> 8) & 0xFF) / 255.0f, (rgb & 0xFF) / 255.0f, a);
}

ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t) {
    return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}

ImU32 U32(const ImVec4& c) { return ImGui::GetColorU32(c); }

// Palette
const unsigned kBg = 0x0E0F13;
const unsigned kCard = 0x16181F;
const unsigned kFrame = 0x1F222C;
const unsigned kFrameHover = 0x282C38;
const unsigned kTrack = 0x2A2E3B;
const unsigned kText = 0xE6E8EE;
const unsigned kMuted = 0x8A8F9C;
const unsigned kAccent = 0x7C86FF;
const unsigned kAccentA = 0x5B8CFF;
const unsigned kAccentB = 0xB26BFF;
const unsigned kGreen = 0x3DD68C;
const unsigned kRed = 0xE5484D;

const char* kInputs[] = {"Default microphone", "Microphone (USB Audio Device)", "Headset Microphone"};
const char* kOutputs[] = {"CABLE Input (VB-Audio Virtual Cable)", "Speakers (preview)"};

void LoadFonts(float scale) {
    ImGuiIO& io = ImGui::GetIO();
    g_titleFont = nullptr;

#ifdef _WIN32
    char dir[MAX_PATH] = {};
    UINT n = GetWindowsDirectoryA(dir, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        char regular[MAX_PATH + 32];
        char bold[MAX_PATH + 32];
        std::snprintf(regular, sizeof(regular), "%s\\Fonts\\segoeui.ttf", dir);
        std::snprintf(bold, sizeof(bold), "%s\\Fonts\\segoeuib.ttf", dir);
        if (GetFileAttributesA(regular) != INVALID_FILE_ATTRIBUTES) {
            io.Fonts->AddFontFromFileTTF(regular, 16.0f * scale);
            if (GetFileAttributesA(bold) != INVALID_FILE_ATTRIBUTES)
                g_titleFont = io.Fonts->AddFontFromFileTTF(bold, 24.0f * scale);
            return;
        }
    }
#endif
    ImFontConfig cfg;
    cfg.SizePixels = 16.0f * scale;
    io.Fonts->AddFontDefault(&cfg);
}

// ---- widgets ---------------------------------------------------------------

bool Toggle(const char* id, bool* v) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = S(40), h = S(22);

    ImGui::InvisibleButton(id, ImVec2(w, h));
    bool changed = false;
    if (ImGui::IsItemClicked()) {
        *v = !*v;
        changed = true;
    }

    ImGuiStorage* st = ImGui::GetStateStorage();
    const ImGuiID gid = ImGui::GetItemID();
    const float target = *v ? 1.0f : 0.0f;
    float t = st->GetFloat(gid, target);
    t += (target - t) * std::min(1.0f, ImGui::GetIO().DeltaTime * 14.0f);
    st->SetFloat(gid, t);

    ImVec4 bg = Mix(Hex(kTrack), Hex(kAccent), t);
    if (ImGui::IsItemHovered()) bg = Mix(bg, Hex(0xFFFFFF), 0.08f);
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), U32(bg), h * 0.5f);

    const float r = h * 0.5f - S(3);
    const float cx = p.x + h * 0.5f + (w - h) * t;
    dl->AddCircleFilled(ImVec2(cx, p.y + h * 0.5f), r, U32(Hex(0xFFFFFF)), 24);
    return changed;
}

// Horizontal integer slider. If `centered`, the fill grows from the middle (for +/- ranges).
bool Slider(const char* id, int* v, int lo, int hi, bool centered, int resetValue) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;
    const float h = S(28);

    ImGui::InvisibleButton(id, ImVec2(w, h));
    const bool active = ImGui::IsItemActive();
    const bool hovered = ImGui::IsItemHovered();

    const float pad = S(10);
    const float x0 = p.x + pad, x1 = p.x + w - pad;
    bool changed = false;

    if (active) {
        float t = std::clamp((ImGui::GetIO().MousePos.x - x0) / (x1 - x0), 0.0f, 1.0f);
        int nv = lo + static_cast<int>(std::lround(t * (hi - lo)));
        if (nv != *v) {
            *v = nv;
            changed = true;
        }
    }
    if (hovered && ImGui::IsMouseDoubleClicked(0) && *v != resetValue) {
        *v = resetValue;
        changed = true;
    }

    const float t = static_cast<float>(*v - lo) / static_cast<float>(hi - lo);
    const float cy = p.y + h * 0.5f;
    const float th = S(5);
    const float kx = x0 + (x1 - x0) * t;
    const float fx = centered ? x0 + (x1 - x0) * 0.5f : x0;

    dl->AddRectFilled(ImVec2(x0, cy - th * 0.5f), ImVec2(x1, cy + th * 0.5f), U32(Hex(kTrack)), th * 0.5f);
    dl->AddRectFilled(ImVec2(std::min(fx, kx), cy - th * 0.5f), ImVec2(std::max(fx, kx), cy + th * 0.5f),
                      U32(Hex(kAccent)), th * 0.5f);

    const float r = (active || hovered) ? S(9) : S(8);
    dl->AddCircleFilled(ImVec2(kx, cy), r, U32(Hex(0xFFFFFF)), 24);
    return changed;
}

// Label on the left, optional value text on the right, on one line.
void Row(const char* label, const char* value, bool accentValue = false) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    const float right = p.x + ImGui::GetContentRegionAvail().x;
    ImGui::TextUnformatted(label);
    if (value) {
        ImVec2 ts = ImGui::CalcTextSize(value);
        ImGui::GetWindowDrawList()->AddText(ImVec2(right - ts.x, p.y), U32(Hex(accentValue ? kAccent : kMuted)), value);
    }
}

void Muted(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, Hex(kMuted));
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
}

void BeginCard(const char* id) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Hex(kCard));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, S(14));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(S(18), S(16)));
    ImGui::BeginChild(id, ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

void EndCard() { ImGui::EndChild(); }

// Card title with an on/off switch on the right. Returns the switch state.
void CardHeader(const char* title, const char* toggleId, bool* enabled) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    const float right = p.x + ImGui::GetContentRegionAvail().x;
    const float h = S(22);

    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + (h - ImGui::GetTextLineHeight()) * 0.5f));
    ImGui::TextUnformatted(title);

    ImGui::SetCursorScreenPos(ImVec2(right - S(40), p.y));
    Toggle(toggleId, enabled);
    ImGui::Dummy(ImVec2(0, S(2)));
}

void Combo(const char* id, int* current, const char* const* items, int count) {
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo(id, items[*current])) {
        for (int i = 0; i < count; ++i) {
            const bool sel = (*current == i);
            if (ImGui::Selectable(items[i], sel)) *current = i;
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}

void DrawLogo(ImVec2 pos, float size, bool active) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    static const float base[5] = {0.40f, 0.70f, 1.00f, 0.60f, 0.30f};
    const float bw = size * 0.14f;
    const float gap = size * 0.115f;
    const float total = 5 * bw + 4 * gap;
    const float x0 = pos.x + (size - total) * 0.5f;
    const float cy = pos.y + size * 0.5f;
    const float t = static_cast<float>(ImGui::GetTime());

    for (int i = 0; i < 5; ++i) {
        float k = base[i];
        if (active) k *= 0.55f + 0.45f * std::sin(t * 4.0f + i * 0.9f);
        const float hh = std::max(size * k, bw);
        const float x = x0 + i * (bw + gap);
        ImVec4 c = Mix(Hex(kAccentA), Hex(kAccentB), i / 4.0f);
        dl->AddRectFilled(ImVec2(x, cy - hh * 0.5f), ImVec2(x + bw, cy + hh * 0.5f), U32(c), bw * 0.5f);
    }
}

}  // namespace

// ---- public ----------------------------------------------------------------

void ApplyTheme(float scale) {
    g_scale = scale;
    LoadFonts(scale);

    ImGuiStyle& s = ImGui::GetStyle();
    s = ImGuiStyle();
    s.WindowBorderSize = 0;
    s.ChildBorderSize = 0;
    s.PopupBorderSize = 1;
    s.FrameBorderSize = 0;
    s.WindowRounding = 0;
    s.FrameRounding = 9;
    s.PopupRounding = 10;
    s.ChildRounding = 14;
    s.FramePadding = ImVec2(12, 9);
    s.ItemSpacing = ImVec2(10, 12);
    s.ItemInnerSpacing = ImVec2(8, 6);
    s.ScrollbarSize = 8;
    s.ScrollbarRounding = 8;
    s.WindowPadding = ImVec2(10, 10);
    s.ScaleAllSizes(scale);

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = Hex(kText);
    c[ImGuiCol_TextDisabled] = Hex(kMuted);
    c[ImGuiCol_WindowBg] = Hex(kBg);
    c[ImGuiCol_ChildBg] = Hex(kCard);
    c[ImGuiCol_PopupBg] = Hex(0x1B1E27);
    c[ImGuiCol_Border] = Hex(0x2A2D38);
    c[ImGuiCol_FrameBg] = Hex(kFrame);
    c[ImGuiCol_FrameBgHovered] = Hex(kFrameHover);
    c[ImGuiCol_FrameBgActive] = Hex(kFrameHover);
    c[ImGuiCol_Button] = Hex(kFrame);
    c[ImGuiCol_ButtonHovered] = Hex(kFrameHover);
    c[ImGuiCol_ButtonActive] = Hex(kTrack);
    c[ImGuiCol_Header] = Hex(kAccent, 0.25f);
    c[ImGuiCol_HeaderHovered] = Hex(kAccent, 0.35f);
    c[ImGuiCol_HeaderActive] = Hex(kAccent, 0.45f);
    c[ImGuiCol_ScrollbarBg] = Hex(kBg, 0.0f);
    c[ImGuiCol_ScrollbarGrab] = Hex(kTrack);
    c[ImGuiCol_ScrollbarGrabHovered] = Hex(kFrameHover);
    c[ImGuiCol_ScrollbarGrabActive] = Hex(kMuted);
    c[ImGuiCol_TextSelectedBg] = Hex(kAccent, 0.35f);
}

void DrawUI(AppState& st) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(S(22), S(22)));
    ImGui::Begin("##main", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar();

    // Header: logo, name, status
    {
        const float logo = S(44);
        DrawLogo(ImGui::GetCursorScreenPos(), logo, st.running);
        ImGui::Dummy(ImVec2(logo, logo));
        ImGui::SameLine(0, S(14));

        ImGui::BeginGroup();
        ImGui::PushFont(g_titleFont ? g_titleFont : ImGui::GetFont());
        ImGui::TextUnformatted("WinEffects");
        ImGui::PopFont();

        ImVec2 p = ImGui::GetCursorScreenPos();
        const float lh = ImGui::GetTextLineHeight();
        const ImVec4 dot = Hex(st.running ? kGreen : kMuted);
        ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(p.x + S(4), p.y + lh * 0.5f), S(4), U32(dot), 16);
        ImGui::SetCursorScreenPos(ImVec2(p.x + S(16), p.y));
        ImGui::TextColored(dot, st.running ? "Running" : "Stopped");
        ImGui::EndGroup();
    }
    ImGui::Dummy(ImVec2(0, S(4)));

    // Devices
    BeginCard("devices");
    Muted("Input");
    Combo("##in", &st.inputDevice, kInputs, IM_ARRAYSIZE(kInputs));
    Muted("Output");
    Combo("##out", &st.outputDevice, kOutputs, IM_ARRAYSIZE(kOutputs));
    Muted("Virtual microphone name");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##mic", "Device name", st.micName, sizeof(st.micName));
    EndCard();

    // Noise suppression
    BeginCard("noise");
    CardHeader("Noise suppression", "##noise_on", &st.noiseEnabled);
    ImGui::BeginDisabled(!st.noiseEnabled);
    char val[32];
    std::snprintf(val, sizeof(val), "%d%%", st.noiseStrength);
    Row("Strength", val, true);
    Slider("##noise_strength", &st.noiseStrength, 0, 100, false, 80);
    ImGui::EndDisabled();
    EndCard();

    // Pitch
    BeginCard("pitch");
    CardHeader("Pitch", "##pitch_on", &st.pitchEnabled);
    ImGui::BeginDisabled(!st.pitchEnabled);
    std::snprintf(val, sizeof(val), "%+d st", st.pitch);
    Row("Shift", val, true);
    Slider("##pitch_value", &st.pitch, -12, 12, true, 0);

    static const int presets[] = {-6, -3, 0, 3, 6};
    const int n = IM_ARRAYSIZE(presets);
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float bw = (ImGui::GetContentRegionAvail().x - spacing * (n - 1)) / n;
    for (int i = 0; i < n; ++i) {
        if (i) ImGui::SameLine();
        const bool sel = (st.pitch == presets[i]);
        if (sel) {
            ImGui::PushStyleColor(ImGuiCol_Button, Hex(kAccent, 0.9f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Hex(kAccent));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Hex(kAccent));
        }
        char lbl[16];
        if (presets[i] == 0)
            std::snprintf(lbl, sizeof(lbl), "0##p%d", i);
        else
            std::snprintf(lbl, sizeof(lbl), "%+d##p%d", presets[i], i);
        if (ImGui::Button(lbl, ImVec2(bw, 0))) st.pitch = presets[i];
        if (sel) ImGui::PopStyleColor(3);
    }
    ImGui::EndDisabled();
    EndCard();

    // Start / Stop
    ImGui::Dummy(ImVec2(0, S(2)));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, S(12));
    if (st.running) {
        ImGui::PushStyleColor(ImGuiCol_Button, Hex(kRed, 0.18f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Hex(kRed, 0.28f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, Hex(kRed, 0.35f));
        ImGui::PushStyleColor(ImGuiCol_Text, Hex(kRed));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, Hex(kAccent));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Mix(Hex(kAccent), Hex(0xFFFFFF), 0.12f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, Mix(Hex(kAccent), Hex(0x000000), 0.12f));
        ImGui::PushStyleColor(ImGuiCol_Text, Hex(0xFFFFFF));
    }
    if (ImGui::Button(st.running ? "Stop" : "Start", ImVec2(-FLT_MIN, S(46)))) st.running = !st.running;
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();

    ImGui::PushStyleColor(ImGuiCol_Text, Hex(kMuted));
    ImGui::TextWrapped("Preview build: the audio engine and virtual microphone are not implemented yet.");
    ImGui::PopStyleColor();

    ImGui::End();
}
