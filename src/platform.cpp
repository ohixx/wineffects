#include "platform.h"

#include <shellapi.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace {

const wchar_t* kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t* kRunValue = L"WinEffects";
const UINT kTrayId = 1;

}  // namespace

bool IsAutostartEnabled() {
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return false;
    const bool exists = RegQueryValueExW(key, kRunValue, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
    RegCloseKey(key);
    return exists;
}

void SetAutostart(bool enabled) {
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) return;
    if (enabled) {
        wchar_t path[MAX_PATH];
        const DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
        if (n > 0 && n < MAX_PATH) {
            const std::wstring cmd = std::wstring(L"\"") + path + L"\" --minimized";
            RegSetValueExW(key, kRunValue, 0, REG_SZ, reinterpret_cast<const BYTE*>(cmd.c_str()),
                           static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t)));
        }
    } else {
        RegDeleteValueW(key, kRunValue);
    }
    RegCloseKey(key);
}

HICON CreateAppIcon(int size) {
    BITMAPV5HEADER bi = {};
    bi.bV5Size = sizeof(bi);
    bi.bV5Width = size;
    bi.bV5Height = -size;  // top-down
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask = 0x00FF0000;
    bi.bV5GreenMask = 0x0000FF00;
    bi.bV5BlueMask = 0x000000FF;
    bi.bV5AlphaMask = 0xFF000000;

    void* bits = nullptr;
    HDC dc = GetDC(nullptr);
    HBITMAP color = CreateDIBSection(dc, reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, dc);
    if (!color || !bits) return nullptr;

    // Five capsules: accent green fading to cyan, same as the in-app logo.
    static const float heights[5] = {0.40f, 0.70f, 1.00f, 0.60f, 0.30f};
    const float s = static_cast<float>(size);
    const float bw = s * 0.14f, gap = s * 0.115f;
    const float total = 5 * bw + 4 * gap;
    const float x0 = (s - total) * 0.5f;

    DWORD* px = static_cast<DWORD*>(bits);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float alpha = 0, r = 0, g = 0, b = 0;
            for (int i = 0; i < 5; ++i) {
                const float cx = x0 + i * (bw + gap) + bw * 0.5f;
                const float hh = std::max(s * 0.9f * heights[i], bw) * 0.5f;
                const float cy = s * 0.5f;
                // distance to a vertical capsule
                const float dy = std::max(std::fabs(y + 0.5f - cy) - (hh - bw * 0.5f), 0.0f);
                const float dx = x + 0.5f - cx;
                const float d = std::sqrt(dx * dx + dy * dy) - bw * 0.5f;
                const float a = std::clamp(0.5f - d, 0.0f, 1.0f);
                if (a > alpha) {
                    const float t = i / 4.0f;
                    r = 0x34 + (0x22 - 0x34) * t;
                    g = 0xD3 + (0xB8 - 0xD3) * t;
                    b = 0x99 + (0xCF - 0x99) * t;
                    alpha = a;
                }
            }
            px[y * size + x] = (static_cast<DWORD>(alpha * 255) << 24) | (static_cast<DWORD>(r) << 16) |
                               (static_cast<DWORD>(g) << 8) | static_cast<DWORD>(b);
        }
    }

    HBITMAP mask = CreateBitmap(size, size, 1, 1, nullptr);
    ICONINFO ii = {};
    ii.fIcon = TRUE;
    ii.hbmMask = mask;
    ii.hbmColor = color;
    HICON icon = CreateIconIndirect(&ii);
    DeleteObject(mask);
    DeleteObject(color);
    return icon;
}

void TrayAdd(HWND hwnd, HICON icon) {
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = kTrayId;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = icon;
    lstrcpynW(nid.szTip, L"WinEffects", ARRAYSIZE(nid.szTip));
    Shell_NotifyIconW(NIM_ADD, &nid);
}

void TrayRemove(HWND hwnd) {
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = kTrayId;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

TrayCommand TrayShowMenu(HWND hwnd, bool running) {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, kTrayOpen, L"Open WinEffects");
    AppendMenuW(menu, MF_STRING, kTrayToggle, running ? L"Pause processing" : L"Resume processing");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kTrayQuit, L"Quit");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);  // required so the menu closes when clicking elsewhere
    const int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
    PostMessage(hwnd, WM_NULL, 0, 0);
    DestroyMenu(menu);
    return static_cast<TrayCommand>(cmd);
}
