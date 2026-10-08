#include "ui.h"

#include <windows.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <shellapi.h>

#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include "platform.h"

namespace {

// ---- style -----------------------------------------------------------------

float g_scale = 1.0f;
ImFont* g_bold = nullptr;
ImFont* g_small = nullptr;
ImFont* g_title = nullptr;

float S(float v) { return v * g_scale; }

ImVec4 Hex(unsigned rgb, float a = 1.0f) {
    return ImVec4(((rgb >> 16) & 0xFF) / 255.0f, ((rgb >> 8) & 0xFF) / 255.0f, (rgb & 0xFF) / 255.0f, a);
}
ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t) {
    return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}
ImU32 U32(const ImVec4& c) { return ImGui::GetColorU32(c); }  // respects BeginDisabled()

const unsigned kBg = 0x111214;
const unsigned kPanel = 0x17191C;
const unsigned kRaised = 0x1F2226;
const unsigned kRaisedHover = 0x272A2F;
const unsigned kBorder = 0x2A2D32;
const unsigned kText = 0xE6E8EB;
const unsigned kMuted = 0x8B9098;
const unsigned kAccent = 0x34D399;
const unsigned kAccent2 = 0x22B8CF;
const unsigned kOnAccent = 0x06130E;
const unsigned kDanger = 0xF2615F;
const unsigned kWarn = 0xF5B83D;

void LoadFonts(float scale) {
    ImGuiIO& io = ImGui::GetIO();
    g_bold = g_small = g_title = nullptr;

    char dir[MAX_PATH] = {};
    const UINT n = GetWindowsDirectoryA(dir, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        struct Family {
            const char* regular;
            const char* bold;
        };
        static const Family families[] = {
            {"segoeui.ttf", "segoeuib.ttf"}, {"tahoma.ttf", "tahomabd.ttf"}, {"arial.ttf", "arialbd.ttf"}};
        for (const Family& f : families) {
            char regular[MAX_PATH + 32], bold[MAX_PATH + 32];
            std::snprintf(regular, sizeof(regular), "%s\\Fonts\\%s", dir, f.regular);
            std::snprintf(bold, sizeof(bold), "%s\\Fonts\\%s", dir, f.bold);
            if (GetFileAttributesA(regular) == INVALID_FILE_ATTRIBUTES) continue;

            const ImWchar* ranges = io.Fonts->GetGlyphRangesCyrillic();  // device names are often Russian
            io.Fonts->AddFontFromFileTTF(regular, 15.0f * scale, nullptr, ranges);
            if (GetFileAttributesA(bold) != INVALID_FILE_ATTRIBUTES) {
                g_bold = io.Fonts->AddFontFromFileTTF(bold, 15.0f * scale, nullptr, ranges);
                g_small = io.Fonts->AddFontFromFileTTF(bold, 11.5f * scale, nullptr, ranges);
                g_title = io.Fonts->AddFontFromFileTTF(bold, 22.0f * scale, nullptr, ranges);
            }
            return;
        }
    }
    ImFontConfig cfg;
    cfg.SizePixels = 15.0f * scale;
    io.Fonts->AddFontDefault(&cfg);
}

ImFont* Or(ImFont* f) { return f ? f : ImGui::GetFont(); }

// ---- small helpers -----------------------------------------------------------

void HandCursor() {
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
}

void Label(const char* text, unsigned color = kMuted) {
    ImGui::PushStyleColor(ImGuiCol_Text, Hex(color));
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
}

void SectionLabel(const char* text) {
    ImGui::PushFont(Or(g_small));
    Label(text);
    ImGui::PopFont();
}

// ---- widgets -----------------------------------------------------------------

enum class Icon { Close, Up, Down, Gear };

bool IconButton(const char* id, Icon icon, float size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton(id, ImVec2(size, size));
    const bool hover = ImGui::IsItemHovered();
    HandCursor();

    if (hover) dl->AddRectFilled(p, ImVec2(p.x + size, p.y + size), U32(Hex(kRaised)), S(6));
    const ImU32 col = U32(Hex(hover ? kText : kMuted));
    const ImVec2 c(p.x + size * 0.5f, p.y + size * 0.5f);
    const float t = S(1.6f);

    switch (icon) {
        case Icon::Close: {
            const float r = size * 0.18f;
            dl->AddLine(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), col, t);
            dl->AddLine(ImVec2(c.x - r, c.y + r), ImVec2(c.x + r, c.y - r), col, t);
            break;
        }
        case Icon::Up:
        case Icon::Down: {
            const float r = size * 0.2f;
            const float d = (icon == Icon::Up) ? -1.0f : 1.0f;
            const ImVec2 pts[3] = {ImVec2(c.x - r, c.y - d * r * 0.5f), ImVec2(c.x, c.y + d * r * 0.5f),
                                   ImVec2(c.x + r, c.y - d * r * 0.5f)};
            dl->AddPolyline(pts, 3, col, ImDrawFlags_None, t);
            break;
        }
        case Icon::Gear: {
            dl->AddCircle(c, size * 0.17f, col, 20, S(2.0f));
            for (int k = 0; k < 8; ++k) {
                const float a = k * 3.14159265f / 4.0f;
                const ImVec2 d(std::cos(a), std::sin(a));
                dl->AddLine(ImVec2(c.x + d.x * size * 0.22f, c.y + d.y * size * 0.22f),
                            ImVec2(c.x + d.x * size * 0.31f, c.y + d.y * size * 0.31f), col, S(2.6f));
            }
            break;
        }
    }
    return pressed;
}

enum class Look { Normal, Primary, Danger };

bool Button(const char* label, ImVec2 size, Look look = Look::Normal) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton(label, size);
    const bool hover = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();
    HandCursor();

    ImVec4 bg, border, text;
    switch (look) {
        case Look::Primary:
            bg = Mix(Hex(kAccent), Hex(0xFFFFFF), hover ? 0.12f : 0.0f);
            if (held) bg = Mix(Hex(kAccent), Hex(0x000000), 0.15f);
            border = bg;
            text = Hex(kOnAccent);
            break;
        case Look::Danger:
            bg = Hex(kDanger, hover ? 0.20f : 0.10f);
            border = Hex(kDanger, 0.55f);
            text = Hex(kDanger);
            break;
        default:
            bg = Hex(hover ? kRaisedHover : kRaised);
            border = Hex(kBorder);
            text = Hex(kText);
            break;
    }
    dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), U32(bg), S(7));
    dl->AddRect(p, ImVec2(p.x + size.x, p.y + size.y), U32(border), S(7));

    ImGui::PushFont(Or(g_bold));
    const ImVec2 ts = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2(p.x + (size.x - ts.x) * 0.5f, p.y + (size.y - ts.y) * 0.5f), U32(text), label);
    ImGui::PopFont();
    return pressed;
}

bool Toggle(const char* id, bool* v) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = S(36), h = S(20);

    ImGui::InvisibleButton(id, ImVec2(w, h));
    HandCursor();
    bool changed = false;
    if (ImGui::IsItemClicked()) {
        *v = !*v;
        changed = true;
    }

    ImGuiStorage* st = ImGui::GetStateStorage();
    const ImGuiID gid = ImGui::GetItemID();
    const float target = *v ? 1.0f : 0.0f;
    float t = st->GetFloat(gid, target);
    t += (target - t) * std::min(1.0f, ImGui::GetIO().DeltaTime * 16.0f);
    st->SetFloat(gid, t);

    ImVec4 bg = Mix(Hex(0x32363C), Hex(kAccent), t);
    if (ImGui::IsItemHovered()) bg = Mix(bg, Hex(0xFFFFFF), 0.08f);
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), U32(bg), h * 0.5f);

    const float r = h * 0.5f - S(3);
    const float cx = p.x + h * 0.5f + (w - h) * t;
    dl->AddCircleFilled(ImVec2(cx, p.y + h * 0.5f), r, U32(Mix(Hex(0xC9CDD3), Hex(kOnAccent), t * 0.9f)), 24);
    return changed;
}

bool Slider(const char* id, int* v, int lo, int hi, bool centered, int resetValue) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;
    const float h = S(24);

    ImGui::InvisibleButton(id, ImVec2(w, h));
    const bool active = ImGui::IsItemActive();
    const bool hover = ImGui::IsItemHovered();
    HandCursor();

    const float pad = S(8);
    const float x0 = p.x + pad, x1 = p.x + w - pad;
    bool changed = false;
    if (active) {
        const float t = std::clamp((ImGui::GetIO().MousePos.x - x0) / (x1 - x0), 0.0f, 1.0f);
        const int nv = lo + static_cast<int>(std::lround(t * (hi - lo)));
        if (nv != *v) {
            *v = nv;
            changed = true;
        }
    }
    if (hover && ImGui::IsMouseDoubleClicked(0) && *v != resetValue) {
        *v = resetValue;
        changed = true;
    }

    const float t = static_cast<float>(*v - lo) / static_cast<float>(hi - lo);
    const float cy = p.y + h * 0.5f;
    const float th = S(4);
    const float kx = x0 + (x1 - x0) * t;
    const float fx = centered ? x0 + (x1 - x0) * 0.5f : x0;

    dl->AddRectFilled(ImVec2(x0, cy - th * 0.5f), ImVec2(x1, cy + th * 0.5f), U32(Hex(0x2D3035)), th * 0.5f);
    dl->AddRectFilled(ImVec2(std::min(fx, kx), cy - th * 0.5f), ImVec2(std::max(fx, kx), cy + th * 0.5f),
                      U32(Hex(kAccent)), th * 0.5f);
    dl->AddCircleFilled(ImVec2(kx, cy), (active || hover) ? S(7.5f) : S(6.5f), U32(Hex(0xF1F3F5)), 24);
    return changed;
}

// Label on the left, value on the right.
void ValueRow(const char* label, const char* value) {
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float right = p.x + ImGui::GetContentRegionAvail().x;
    ImGui::TextUnformatted(label);
    const ImVec2 ts = ImGui::CalcTextSize(value);
    ImGui::GetWindowDrawList()->AddText(ImVec2(right - ts.x, p.y), U32(Hex(kAccent)), value);
}

void LevelBar(const char* label, ImVec2 p, float width, float level) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::PushFont(Or(g_small));
    dl->AddText(ImVec2(p.x, p.y), U32(Hex(kMuted)), label);
    ImGui::PopFont();

    const float db = 20.0f * std::log10(std::max(level, 1e-5f));
    const float v = std::clamp((db + 60.0f) / 60.0f, 0.0f, 1.0f);
    const float x0 = p.x + S(34), y0 = p.y + S(4), h = S(6);
    dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + width, y0 + h), U32(Hex(0x2D3035)), h * 0.5f);
    if (v > 0.01f) {
        const ImVec4 c = v > 0.93f ? Hex(kDanger) : (v > 0.82f ? Hex(kWarn) : Hex(kAccent));
        dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + std::max(width * v, h), y0 + h), U32(c), h * 0.5f);
    }
}

void DrawLogo(ImVec2 pos, float size, float level, bool active) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    static const float base[5] = {0.40f, 0.70f, 1.00f, 0.60f, 0.30f};
    const float bw = size * 0.14f, gap = size * 0.115f;
    const float total = 5 * bw + 4 * gap;
    const float x0 = pos.x + (size - total) * 0.5f;
    const float cy = pos.y + size * 0.5f;
    const float t = static_cast<float>(ImGui::GetTime());

    for (int i = 0; i < 5; ++i) {
        float k = base[i];
        if (active) k *= 0.45f + 0.55f * std::clamp(level * 3.0f, 0.0f, 1.0f) * (0.65f + 0.35f * std::sin(t * 7 + i));
        const float hh = std::max(size * 0.9f * k, bw);
        const float x = x0 + i * (bw + gap);
        dl->AddRectFilled(ImVec2(x, cy - hh * 0.5f), ImVec2(x + bw, cy + hh * 0.5f),
                          U32(Mix(Hex(kAccent), Hex(kAccent2), i / 4.0f)), bw * 0.5f);
    }
}

// Rounded container that grows with its content.
bool BeginCard(const char* id, bool danger = false) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Hex(kPanel));
    ImGui::PushStyleColor(ImGuiCol_Border, danger ? Hex(kDanger, 0.6f) : Hex(kBorder));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, S(9));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(S(16), S(14)));
    const bool open = ImGui::BeginChild(
        id, ImVec2(0, 0),
        ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_Borders,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    return open;
}

void DeviceCombo(const char* id, std::string& selected, const std::vector<std::string>& names, float width,
                 bool& changed) {
    ImGui::SetNextItemWidth(width);
    if (ImGui::BeginCombo(id, selected.empty() ? "System default" : selected.c_str())) {
        if (ImGui::Selectable("System default", selected.empty()) && !selected.empty()) {
            selected.clear();
            changed = true;
        }
        for (const std::string& n : names) {
            const bool sel = (n == selected);
            if (ImGui::Selectable(n.c_str(), sel) && !sel) {
                selected = n;
                changed = true;
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}

// A settings line: title + hint on the left, control on the right.
template <typename F>
void SettingRow(const char* title, const char* hint, float controlWidth, F control) {
    const float x0 = ImGui::GetCursorPosX();
    const float y0 = ImGui::GetCursorPosY();
    const float avail = ImGui::GetContentRegionAvail().x;
    const float frameH = ImGui::GetFrameHeight();

    ImGui::BeginGroup();
    ImGui::PushTextWrapPos(ImGui::GetCursorScreenPos().x + avail - controlWidth - S(20));
    ImGui::TextUnformatted(title);
    if (hint) Label(hint);
    ImGui::PopTextWrapPos();
    ImGui::EndGroup();
    const float h = std::max(ImGui::GetItemRectSize().y, frameH);

    ImGui::SetCursorPos(ImVec2(x0 + avail - controlWidth, y0 + (h - frameH) * 0.5f));
    control(frameH);

    ImGui::SetCursorPos(ImVec2(x0, y0 + h + S(14)));
    ImGui::Dummy(ImVec2(0, 0));
}

// ---- pages -------------------------------------------------------------------

enum class Page { Effects, Settings };
Page g_page = Page::Effects;

void DrawEffectParams(App& app, Effect& e) {
    ImGui::BeginDisabled(!e.enabled.load());
    for (Param& p : e.params) {
        ImGui::PushID(p.key.c_str());
        int v = p.value.load();
        char text[32];
        if (!p.choices.empty())
            std::snprintf(text, sizeof(text), "%s", p.choices[static_cast<size_t>(std::clamp(v, 0, static_cast<int>(p.choices.size()) - 1))].c_str());
        else
            std::snprintf(text, sizeof(text), p.format.c_str(), v);
        ValueRow(p.label.c_str(), text);
        if (Slider("##slider", &v, p.min, p.max, p.centered, p.def)) {
            p.value.store(v);
            app.dirty = true;
        }
        ImGui::PopID();
    }
    ImGui::EndDisabled();
}

void DrawAddMenu(App& app, const std::vector<std::shared_ptr<Effect>>& chain) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(S(6), S(6)));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, S(9));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1);
    ImGui::SetNextWindowSize(ImVec2(S(340), 0));
    const bool open = ImGui::BeginPopup("add_effect");
    ImGui::PopStyleVar(3);
    if (!open) return;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (const EffectInfo& info : AvailableEffects()) {
        bool added = false;
        for (const auto& e : chain) added = added || std::string(e->id()) == info.id;

        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float w = ImGui::GetContentRegionAvail().x;
        ImGui::BeginDisabled(added);
        const bool clicked = ImGui::Selectable((std::string("##") + info.id).c_str(), false, 0, ImVec2(0, S(48)));
        ImGui::EndDisabled();

        ImGui::PushFont(Or(g_bold));
        dl->AddText(ImVec2(p.x + S(10), p.y + S(6)), U32(Hex(added ? kMuted : kText)), info.title);
        ImGui::PopFont();
        dl->AddText(ImVec2(p.x + S(10), p.y + S(26)), U32(Hex(kMuted)), info.description);
        if (added) {
            const ImVec2 ts = ImGui::CalcTextSize("Added");
            dl->AddText(ImVec2(p.x + w - ts.x - S(10), p.y + S(6)), U32(Hex(kMuted)), "Added");
        }

        if (clicked) {
            app.engine.addEffect(info.id);
            app.dirty = true;
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::EndPopup();
}

void DrawNoCableBanner(App& app) {
    BeginCard("##nocable");
    ImGui::PushFont(Or(g_bold));
    ImGui::TextUnformatted("No virtual microphone found");
    ImGui::PopFont();
    ImGui::PushStyleColor(ImGuiCol_Text, Hex(kMuted));
    ImGui::TextWrapped(
        "WinEffects sends the processed sound to a virtual cable, which other programs see as a microphone. "
        "Install VB-Cable (free), then press Check again.");
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, S(2)));
    if (Button("Get VB-Cable", ImVec2(S(126), S(30)), Look::Primary))
        ShellExecuteW(nullptr, L"open", L"https://vb-audio.com/Cable/", nullptr, nullptr, SW_SHOWNORMAL);
    ImGui::SameLine();
    if (Button("Check again", ImVec2(S(110), S(30)))) {
        app.engine.refreshDevices();
        app.selectVirtualCable();
    }
    ImGui::EndChild();
    ImGui::Dummy(ImVec2(0, S(2)));
}

// The virtual cable exists, but the sound is being sent somewhere else (speakers, headphones...).
void DrawWrongOutputBanner(App& app, const std::string& cable) {
    BeginCard("##wrongout", true);
    ImGui::PushFont(Or(g_bold));
    ImGui::TextUnformatted("The sound is not going to the virtual microphone");
    ImGui::PopFont();
    ImGui::PushStyleColor(ImGuiCol_Text, Hex(kMuted));
    ImGui::TextWrapped("Output is set to \"%s\", so Discord and other apps cannot hear WinEffects. "
                       "Send it to the virtual cable instead.",
                       app.settings.output.empty() ? "System default" : app.settings.output.c_str());
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, S(2)));
    if (Button("Use CABLE Input", ImVec2(S(150), S(30)), Look::Primary)) {
        app.settings.output = cable;
        app.dirty = true;
        app.restartIfRunning();
    }
    ImGui::EndChild();
    ImGui::Dummy(ImVec2(0, S(2)));
}

void DrawEffectsPage(App& app) {
    const std::string cable = app.virtualCableName();
    if (cable.empty())
        DrawNoCableBanner(app);
    else if (app.settings.output.find("CABLE") == std::string::npos)
        DrawWrongOutputBanner(app, cable);
    // Errors from the last Start attempt.
    if (!app.error.empty()) {
        BeginCard("##error", true);
        ImGui::PushStyleColor(ImGuiCol_Text, Hex(kDanger));
        ImGui::TextWrapped("%s", app.error.c_str());
        ImGui::PopStyleColor();
        Label("Click to dismiss. Check the devices in Settings.");
        ImGui::EndChild();
        if (ImGui::IsItemClicked()) app.error.clear();
        ImGui::Dummy(ImVec2(0, S(4)));
    }

    // Section header with the Add button.
    const auto chain = app.engine.chain();
    {
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float right = p.x + ImGui::GetContentRegionAvail().x;
        const ImVec2 btn(S(118), S(30));

        ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + (btn.y - ImGui::GetTextLineHeight()) * 0.5f));
        SectionLabel("EFFECTS");

        ImGui::SetCursorScreenPos(ImVec2(right - btn.x, p.y));
        if (Button("+  Add effect", btn)) ImGui::OpenPopup("add_effect");
        const ImVec2 bmin = ImGui::GetItemRectMin(), bmax = ImGui::GetItemRectMax();
        ImGui::SetNextWindowPos(ImVec2(bmax.x, bmax.y + S(6)), ImGuiCond_Always, ImVec2(1, 0));
        DrawAddMenu(app, chain);
        (void)bmin;

        ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + btn.y + S(12)));
        ImGui::Dummy(ImVec2(0, 0));
    }

    if (chain.empty()) {
        ImGui::Dummy(ImVec2(0, S(36)));
        const float w = ImGui::GetContentRegionAvail().x;
        const char* a = "No effects yet";
        const char* b = "Press \"Add effect\" to build your chain.";
        ImGui::PushFont(Or(g_bold));
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (w - ImGui::CalcTextSize(a).x) * 0.5f);
        Label(a, kText);
        ImGui::PopFont();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (w - ImGui::CalcTextSize(b).x) * 0.5f);
        Label(b);
        return;
    }

    int removeIndex = -1, moveIndex = -1, moveDir = 0;
    for (size_t i = 0; i < chain.size(); ++i) {
        Effect& e = *chain[i];
        ImGui::PushID(&e);
        BeginCard("##fx");

        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float right = p.x + ImGui::GetContentRegionAvail().x;

        ImGui::PushFont(Or(g_bold));
        ImGui::TextUnformatted(e.title());
        ImGui::PopFont();
        Label(e.description());
        const ImVec2 after = ImGui::GetCursorScreenPos();

        const float ib = S(26);
        float x = right - S(36);
        ImGui::SetCursorScreenPos(ImVec2(x, p.y + S(1)));
        bool on = e.enabled.load();
        if (Toggle("##on", &on)) {
            e.enabled.store(on);
            app.dirty = true;
        }
        x -= S(12) + ib;
        ImGui::SetCursorScreenPos(ImVec2(x, p.y - S(2)));
        if (IconButton("##remove", Icon::Close, ib)) removeIndex = static_cast<int>(i);
        x -= ib;
        ImGui::SetCursorScreenPos(ImVec2(x, p.y - S(2)));
        ImGui::BeginDisabled(i + 1 >= chain.size());
        if (IconButton("##down", Icon::Down, ib)) {
            moveIndex = static_cast<int>(i);
            moveDir = 1;
        }
        ImGui::EndDisabled();
        x -= ib;
        ImGui::SetCursorScreenPos(ImVec2(x, p.y - S(2)));
        ImGui::BeginDisabled(i == 0);
        if (IconButton("##up", Icon::Up, ib)) {
            moveIndex = static_cast<int>(i);
            moveDir = -1;
        }
        ImGui::EndDisabled();

        ImGui::SetCursorScreenPos(ImVec2(after.x, after.y + S(4)));
        DrawEffectParams(app, e);

        ImGui::EndChild();
        ImGui::PopID();
    }

    if (removeIndex >= 0) {
        app.engine.removeEffect(static_cast<size_t>(removeIndex));
        app.dirty = true;
    } else if (moveIndex >= 0) {
        app.engine.moveEffect(static_cast<size_t>(moveIndex), moveDir);
        app.dirty = true;
    }
}

bool pendingBufferRestart = false;

void DrawSettingsPage(App& app) {
    Settings& s = app.settings;
    const float cw = std::min(S(270), ImGui::GetContentRegionAvail().x * 0.5f);
    bool devicesChanged = false;
    bool restartPending = false;

    ImGui::PushFont(Or(g_title));
    ImGui::TextUnformatted("Settings");
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, S(10)));

    // Audio
    {
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float right = p.x + ImGui::GetContentRegionAvail().x;
        const ImVec2 btn(S(76), S(26));
        ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + (btn.y - ImGui::GetTextLineHeight()) * 0.5f));
        SectionLabel("AUDIO");
        ImGui::SetCursorScreenPos(ImVec2(right - btn.x, p.y));
        if (Button("Refresh", btn)) app.engine.refreshDevices();
        ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + btn.y + S(12)));
        ImGui::Dummy(ImVec2(0, 0));
    }

    SettingRow("Microphone", "Your real microphone.", cw, [&](float) {
        DeviceCombo("##input", s.input, app.engine.inputDevices(), cw, devicesChanged);
    });
    SettingRow("Virtual microphone output", "Where the processed sound goes. Pick the input of a virtual cable, e.g. CABLE Input.",
               cw, [&](float) { DeviceCombo("##output", s.output, app.engine.outputDevices(), cw, devicesChanged); });
    SettingRow("Test the virtual microphone",
               "Plays a 2-second beep into the output. In Windows (Sound > Recording > CABLE Output) or Discord's "
               "mic test the level should move.",
               S(130), [&](float) {
                   ImGui::BeginDisabled(!app.engine.running());
                   if (Button("Play beep", ImVec2(S(130), S(30)))) app.engine.playTestTone();
                   ImGui::EndDisabled();
               });
    SettingRow("Virtual microphone name", "Name for the built-in virtual device (planned). A cable like VB-Cable keeps its own name.",
               cw, [&](float) {
                   ImGui::SetNextItemWidth(cw);
                   if (ImGui::InputText("##micname", &s.micName))
                       app.dirty = true;
               });
    SettingRow("Listen to yourself", "Also play the processed sound through your headphones.", S(36), [&](float fh) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (fh - S(20)) * 0.5f);
        if (Toggle("##monitor", &s.monitorEnabled)) devicesChanged = true;
    });
    {
        char title[48];
        std::snprintf(title, sizeof(title), "Audio buffer: %d ms", s.bufferMs);
        SettingRow(title, "Cushion before the output. Raise it if the sound crackles or stutters; lower it for less delay.", cw, [&](float) {
            ImGui::PushID("##buffer");
            if (Slider("##bufms", &s.bufferMs, 2, 100, false, 5)) {
                app.dirty = true;
                restartPending = true;
            }
            ImGui::PopID();
        });
    }
    SettingRow("Ultra-low latency devices",
               "Asks Windows for the smallest audio period the drivers allow. Turn it off if the sound crackles "
               "even with a bigger buffer.",
               S(36), [&](float fh) {
                   ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (fh - S(20)) * 0.5f);
                   if (Toggle("##lowlat", &s.lowLatencyDevices)) {
                       app.dirty = true;
                       restartPending = true;
                   }
               });
    ImGui::BeginDisabled(!s.monitorEnabled);
    SettingRow("Headphones", nullptr, cw, [&](float) {
        DeviceCombo("##monitor_dev", s.monitor, app.engine.outputDevices(), cw, devicesChanged);
    });
    ImGui::EndDisabled();

    ImGui::Dummy(ImVec2(0, S(8)));
    SectionLabel("GENERAL");
    ImGui::Dummy(ImVec2(0, S(2)));

    auto toggleRow = [&](const char* title, const char* hint, const char* id, bool* v) {
        bool changed = false;
        SettingRow(title, hint, S(36), [&](float fh) {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (fh - S(20)) * 0.5f);
            changed = Toggle(id, v);
        });
        return changed;
    };

    if (toggleRow("Start with Windows", "Launch WinEffects when you sign in.", "##autostart", &s.autostart)) {
        SetAutostart(s.autostart);
        s.autostart = IsAutostartEnabled();
    }
    if (toggleRow("Close to tray", "Closing the window keeps WinEffects running in the tray.", "##tray", &s.closeToTray))
        app.dirty = true;
    if (toggleRow("Start minimized", "Open hidden in the tray instead of showing the window.", "##minimized",
                  &s.startMinimized))
        app.dirty = true;
    if (devicesChanged) {
        app.dirty = true;
        app.restartIfRunning();
    }
    // The buffer size only matters on start; apply it once the slider is released.
    if (restartPending) pendingBufferRestart = true;
    if (pendingBufferRestart && !ImGui::IsMouseDown(0)) {
        pendingBufferRestart = false;
        app.restartIfRunning();
    }
}

void DrawFooter(App& app, ImVec2 origin, float width, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float pad = S(20);
    dl->AddLine(origin, ImVec2(origin.x + width, origin.y), U32(Hex(kBorder)));

    const bool running = app.engine.running();
    const float pitch = S(16);
    const float top = origin.y + (height - (2 * pitch + S(14))) * 0.5f;
    LevelBar("IN", ImVec2(origin.x + pad, top), S(120), app.engine.inputLevel());
    LevelBar("OUT", ImVec2(origin.x + pad, top + pitch), S(120), app.engine.outputLevel());
    LevelBar("SENT", ImVec2(origin.x + pad, top + 2 * pitch), S(120), app.engine.deviceLevel());

    const ImVec2 btn(S(132), S(38));
    const float bx = origin.x + width - pad - btn.x;
    ImGui::SetCursorScreenPos(ImVec2(bx, origin.y + (height - btn.y) * 0.5f));
    ImGui::BeginDisabled(app.paused);
    if (Button(app.soundCheck ? "Stop check" : "Sound check", btn, app.soundCheck ? Look::Danger : Look::Primary))
        app.setSoundCheck(!app.soundCheck);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Hear yourself with all effects applied for 30 seconds.\nUse headphones to avoid an echo.");

    char status[72];
    if (running && app.engine.glitches() > 0)
        std::snprintf(status, sizeof(status), "Live  \xC2\xB7  ~%d ms  \xC2\xB7  %u glitches", app.engine.latencyMs(),
                      app.engine.glitches());
    else if (running)
        std::snprintf(status, sizeof(status), "%s  \xC2\xB7  ~%d ms", app.soundCheck ? "Listening" : "Live",
                      app.engine.latencyMs());
    else
        std::snprintf(status, sizeof(status), "%s", app.paused ? "Paused" : (app.error.empty() ? "Starting..." : "Not started"));
    const ImVec2 ts = ImGui::CalcTextSize(status);
    const ImVec4 col = running ? Hex(kAccent) : Hex(app.error.empty() ? kMuted : kDanger);
    dl->AddText(ImVec2(bx - S(14) - ts.x, origin.y + (height - ts.y) * 0.5f), U32(col), status);
}

}  // namespace

// ---- public ------------------------------------------------------------------

void ApplyTheme(float scale) {
    g_scale = scale;
    LoadFonts(scale);

    ImGuiStyle& s = ImGui::GetStyle();
    s = ImGuiStyle();
    s.WindowBorderSize = 0;
    s.ChildBorderSize = 1;
    s.PopupBorderSize = 1;
    s.FrameBorderSize = 1;
    s.WindowRounding = 0;
    s.FrameRounding = 7;
    s.PopupRounding = 9;
    s.ChildRounding = 9;
    s.GrabRounding = 7;
    s.FramePadding = ImVec2(11, 8);
    s.ItemSpacing = ImVec2(10, 10);
    s.ItemInnerSpacing = ImVec2(8, 6);
    s.ScrollbarSize = 9;
    s.ScrollbarRounding = 9;
    s.WindowPadding = ImVec2(10, 10);
    s.ScaleAllSizes(scale);

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = Hex(kText);
    c[ImGuiCol_TextDisabled] = Hex(kMuted);
    c[ImGuiCol_WindowBg] = Hex(kBg);
    c[ImGuiCol_ChildBg] = Hex(kBg);
    c[ImGuiCol_PopupBg] = Hex(0x1A1C20);
    c[ImGuiCol_Border] = Hex(kBorder);
    c[ImGuiCol_FrameBg] = Hex(kRaised);
    c[ImGuiCol_FrameBgHovered] = Hex(kRaisedHover);
    c[ImGuiCol_FrameBgActive] = Hex(kRaisedHover);
    c[ImGuiCol_Button] = Hex(kRaised);
    c[ImGuiCol_ButtonHovered] = Hex(kRaisedHover);
    c[ImGuiCol_ButtonActive] = Hex(kRaisedHover);
    c[ImGuiCol_Header] = Hex(kAccent, 0.16f);
    c[ImGuiCol_HeaderHovered] = Hex(0xFFFFFF, 0.06f);
    c[ImGuiCol_HeaderActive] = Hex(kAccent, 0.22f);
    c[ImGuiCol_ScrollbarBg] = Hex(kBg, 0.0f);
    c[ImGuiCol_ScrollbarGrab] = Hex(0x33373D);
    c[ImGuiCol_ScrollbarGrabHovered] = Hex(0x484D55);
    c[ImGuiCol_ScrollbarGrabActive] = Hex(kMuted);
    c[ImGuiCol_TextSelectedBg] = Hex(kAccent, 0.30f);
}

void DrawUI(App& app) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::Begin("##main", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar(2);

    static bool wasSettings = false;
    const bool settingsOpen = (g_page == Page::Settings);
    if (settingsOpen && !wasSettings) app.engine.refreshDevices();
    wasSettings = settingsOpen;

    const ImVec2 wp = ImGui::GetWindowPos();
    const float W = ImGui::GetWindowWidth();
    const float Hh = ImGui::GetWindowHeight();
    const float pad = S(20), header = S(56);
    const bool running = app.engine.running();

    // Header: small logo on the left, settings on the right.
    {
        const float logo = S(24);
        DrawLogo(ImVec2(wp.x + pad, wp.y + (header - logo) * 0.5f), logo, app.engine.outputLevel(), running);

        ImGui::SetCursorPos(ImVec2(pad + logo + S(10), (header - ImGui::GetTextLineHeight()) * 0.5f));
        ImGui::PushFont(Or(g_bold));
        ImGui::TextUnformatted("WinEffects");
        ImGui::PopFont();

        const float nav = S(32);
        ImGui::SetCursorPos(ImVec2(W - pad - nav, (header - nav) * 0.5f));
        if (IconButton("##nav", settingsOpen ? Icon::Close : Icon::Gear, nav))
            g_page = settingsOpen ? Page::Effects : Page::Settings;

        ImGui::GetWindowDrawList()->AddLine(ImVec2(wp.x, wp.y + header), ImVec2(wp.x + W, wp.y + header),
                                            U32(Hex(kBorder)));
        ImGui::SetCursorPos(ImVec2(0, header));
    }

    // Body
    const float footer = settingsOpen ? 0.0f : S(76);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pad, S(20)));
    ImGui::BeginChild("##body", ImVec2(0, footer > 0 ? -footer : 0), ImGuiChildFlags_AlwaysUseWindowPadding, 0);
    ImGui::PopStyleVar();
    if (settingsOpen)
        DrawSettingsPage(app);
    else
        DrawEffectsPage(app);
    ImGui::EndChild();

    if (!settingsOpen) DrawFooter(app, ImVec2(wp.x, wp.y + Hh - footer), W, footer);

    ImGui::End();
}
