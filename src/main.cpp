#include <d3d11.h>
#include <dwmapi.h>
#include <windows.h>

#include "app.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "platform.h"
#include "ui.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {

constexpr int kClientWidth = 540, kClientHeight = 680;
constexpr int kMinWidth = 460, kMinHeight = 520;
const wchar_t* kWindowClass = L"WinEffectsWindow";

ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
IDXGISwapChain* g_swapChain = nullptr;
ID3D11RenderTargetView* g_rtv = nullptr;
bool g_occluded = false;
UINT g_resizeW = 0, g_resizeH = 0;

App* g_app = nullptr;
float g_scale = 1.0f;
bool g_quitting = false;
UINT g_taskbarCreated = 0;

void CreateRenderTarget() {
    ID3D11Texture2D* back = nullptr;
    g_swapChain->GetBuffer(0, IID_PPV_ARGS(&back));
    if (back) {
        g_device->CreateRenderTargetView(back, nullptr, &g_rtv);
        back->Release();
    }
}

void CleanupRenderTarget() {
    if (g_rtv) {
        g_rtv->Release();
        g_rtv = nullptr;
    }
}

bool CreateDevice(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL got;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2,
                                               D3D11_SDK_VERSION, &sd, &g_swapChain, &g_device, &got, &g_context);
    if (hr == DXGI_ERROR_UNSUPPORTED) {
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2, D3D11_SDK_VERSION,
                                           &sd, &g_swapChain, &g_device, &got, &g_context);
    }
    if (FAILED(hr)) return false;
    CreateRenderTarget();
    return true;
}

void CleanupDevice() {
    CleanupRenderTarget();
    if (g_swapChain) g_swapChain->Release();
    if (g_context) g_context->Release();
    if (g_device) g_device->Release();
    g_swapChain = nullptr;
    g_context = nullptr;
    g_device = nullptr;
}

void ShowMainWindow(HWND hwnd) {
    ShowWindow(hwnd, IsIconic(hwnd) ? SW_RESTORE : SW_SHOW);
    SetForegroundWindow(hwnd);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) return TRUE;

    if (msg == g_taskbarCreated && g_taskbarCreated != 0) {  // Explorer restarted
        TrayAdd(hwnd, CreateAppIcon(32));
        return 0;
    }

    switch (msg) {
        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED) {
                g_resizeW = LOWORD(lParam);
                g_resizeH = HIWORD(lParam);
            }
            return 0;
        case WM_GETMINMAXINFO: {
            RECT rc = {0, 0, static_cast<LONG>(kMinWidth * g_scale), static_cast<LONG>(kMinHeight * g_scale)};
            AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
            auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
            mmi->ptMinTrackSize.x = rc.right - rc.left;
            mmi->ptMinTrackSize.y = rc.bottom - rc.top;
            return 0;
        }
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU) return 0;  // no ALT menu
            break;
        case WM_CLOSE:
            if (g_app && g_app->settings.closeToTray && !g_quitting) {
                ShowWindow(hwnd, SW_HIDE);
                return 0;
            }
            break;
        case WM_TRAYICON:
            if (lParam == WM_LBUTTONUP || lParam == WM_LBUTTONDBLCLK) {
                ShowMainWindow(hwnd);
            } else if (lParam == WM_RBUTTONUP && g_app) {
                switch (TrayShowMenu(hwnd, g_app->engine.running())) {
                    case kTrayOpen: ShowMainWindow(hwnd); break;
                    case kTrayToggle: g_app->toggle(); break;
                    case kTrayQuit:
                        g_quitting = true;
                        DestroyWindow(hwnd);
                        break;
                    default: break;
                }
            }
            return 0;
        case WM_DESTROY:
            TrayRemove(hwnd);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    // One instance only: wake the existing one instead.
    HANDLE singleton = CreateMutexW(nullptr, TRUE, L"WinEffects.SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND other = FindWindowW(kWindowClass, nullptr)) PostMessage(other, WM_TRAYICON, 0, WM_LBUTTONUP);
        return 0;
    }
    const bool minimizedArg = wcsstr(GetCommandLineW(), L"--minimized") != nullptr;

    ImGui_ImplWin32_EnableDpiAwareness();
    g_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY));
    g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    HICON bigIcon = CreateAppIcon(48);
    HICON smallIcon = CreateAppIcon(24);

    WNDCLASSEXW wc = {sizeof(wc)};
    wc.style = CS_CLASSDC | CS_DBLCLKS;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = bigIcon;
    wc.hIconSm = smallIcon;
    wc.lpszClassName = kWindowClass;
    RegisterClassExW(&wc);

    const DWORD style = WS_OVERLAPPEDWINDOW;
    RECT rc = {0, 0, static_cast<LONG>(kClientWidth * g_scale), static_cast<LONG>(kClientHeight * g_scale)};
    AdjustWindowRect(&rc, style, FALSE);
    const int w = rc.right - rc.left, h = rc.bottom - rc.top;
    const int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    const int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    HWND hwnd = CreateWindowW(kWindowClass, L"WinEffects", style, x, y, w, h, nullptr, nullptr, hInstance, nullptr);

    // Dark title bar (Windows 10 1809+ / 11); ignored where unsupported.
    const BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
    const COLORREF caption = RGB(0x11, 0x12, 0x14);
    DwmSetWindowAttribute(hwnd, 35 /* DWMWA_CAPTION_COLOR */, &caption, sizeof(caption));

    if (!CreateDevice(hwnd)) {
        CleanupDevice();
        UnregisterClassW(kWindowClass, hInstance);
        MessageBoxW(nullptr, L"Failed to initialise Direct3D 11.", L"WinEffects", MB_ICONERROR);
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    ApplyTheme(g_scale);
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_device, g_context);

    App app;
    g_app = &app;
    app.load();
    TrayAdd(hwnd, smallIcon);

    if (app.settings.autoStartEngine) app.start();
    if (!(minimizedArg || app.settings.startMinimized)) {
        ShowWindow(hwnd, SW_SHOWDEFAULT);
        UpdateWindow(hwnd);
    }

    const float clear[4] = {0x11 / 255.0f, 0x12 / 255.0f, 0x14 / 255.0f, 1.0f};

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        // Hidden in the tray or minimized: nothing to draw, sleep until a message arrives.
        if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) {
            MsgWaitForMultipleObjects(0, nullptr, FALSE, 250, QS_ALLINPUT);
            continue;
        }
        if (g_occluded && g_swapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) {
            Sleep(20);
            continue;
        }
        g_occluded = false;

        if (g_resizeW != 0 && g_resizeH != 0) {
            CleanupRenderTarget();
            g_swapChain->ResizeBuffers(0, g_resizeW, g_resizeH, DXGI_FORMAT_UNKNOWN, 0);
            g_resizeW = g_resizeH = 0;
            CreateRenderTarget();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        DrawUI(app);
        ImGui::Render();

        g_context->OMSetRenderTargets(1, &g_rtv, nullptr);
        g_context->ClearRenderTargetView(g_rtv, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_occluded = (g_swapChain->Present(1, 0) == DXGI_STATUS_OCCLUDED);

        if (app.dirty && !ImGui::IsMouseDown(0)) app.save();  // not while dragging a slider
    }

    app.save();
    app.stop();
    g_app = nullptr;

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDevice();
    UnregisterClassW(kWindowClass, hInstance);
    DestroyIcon(bigIcon);
    DestroyIcon(smallIcon);
    CloseHandle(singleton);
    return 0;
}
