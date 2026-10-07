#pragma once

#include <windows.h>

// Start with Windows (HKCU Run key).
bool IsAutostartEnabled();
void SetAutostart(bool enabled);

// Draws the app icon (five equaliser bars) at runtime, so no .ico resource is needed.
HICON CreateAppIcon(int size);

// Notification-area icon.
constexpr UINT WM_TRAYICON = WM_APP + 1;
enum TrayCommand { kTrayNone = 0, kTrayOpen, kTrayToggle, kTrayQuit };

void TrayAdd(HWND hwnd, HICON icon);
void TrayRemove(HWND hwnd);
TrayCommand TrayShowMenu(HWND hwnd, bool running);
