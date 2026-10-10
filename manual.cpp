// MIT; see LICENSE and THIRD_PARTY_NOTICES.md. Experimental manual hardware test.
#include <windows.h>
#include <string>
#include <memory>
#include "manual_rules.h"
namespace {
constexpr int Normal = 101, Boost = 102, Stop = 103;
constexpr UINT Completed = WM_APP + 1;
HWND mainWindow{}, statusWindow{}, normalButton{}, boostButton{}, stopButton{};
HANDLE stopEvent{};
bool active = false, closeAfter = false, eligible = false, boostTouched = false;
HFONT font{};
std::wstring registry_text(const wchar_t* key) {
    wchar_t value[256]{};
    DWORD size = sizeof(value);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\BIOS", key,
        RRF_RT_REG_SZ, nullptr, value, &size) != ERROR_SUCCESS) return {};
    return value;
}
unsigned send_mode(unsigned mode) {
    // No state query: the reference query calibrates by changing mode.
    HANDLE driver = CreateFileW(L"\\\\.\\EnergyDrv", GENERIC_WRITE, 0, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (driver == INVALID_HANDLE_VALUE) return GetLastError();
    DWORD input[3]{6, 1, mode}, returned = 0;
    const BOOL accepted = DeviceIoControl(driver, 0x831020C0u, input, sizeof(input),
        nullptr, 0, &returned, nullptr);
    const DWORD error = accepted ? ERROR_SUCCESS : GetLastError();
    CloseHandle(driver);
    return error;
}
struct Request { bool boost; companion::ManualOutcome outcome; };
DWORD WINAPI worker(void* parameter) {
    auto* request = static_cast<Request*>(parameter);
    request->outcome = companion::manual_sequence(request->boost, send_mode,
        [] { WaitForSingleObject(stopEvent, 3000); });
    PostMessageW(mainWindow, Completed, 0, reinterpret_cast<LPARAM>(request));
    return 0;
}
void status(const std::wstring& text) { SetWindowTextW(statusWindow, text.c_str()); }
void buttons() {
    EnableWindow(normalButton, eligible && !active);
    EnableWindow(boostButton, eligible && !active);
    EnableWindow(stopButton, active);
}
void start(bool boost) {
    if (!eligible || active) return;
    ResetEvent(stopEvent);
    auto request = std::make_unique<Request>();
    request->boost = boost;
    active = true;
    if (boost) boostTouched = true;
    buttons();
    status(boost ? L"Sending Boost. A Normal command follows after up to 3 seconds.\r\nWatch the fans; driver acceptance is not measured fan speed."
                 : L"Sending one Normal command. Watch whether the fans settle.");
    HANDLE thread = CreateThread(nullptr, 0, worker, request.get(), 0, nullptr);
    if (thread) { request.release(); CloseHandle(thread); }
    else { active = false; buttons(); status(L"Could not start command worker; no command sent."); }
}
LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE: {
        mainWindow = window;
        stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        eligible = stopEvent && companion::manual_target(registry_text(L"SystemProductName"), registry_text(L"BIOSVersion"));
        font = CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        auto control = [&](const wchar_t* type, const wchar_t* text, int x, int y, int width, int height, int id) {
            HWND child = CreateWindowW(type, text, WS_CHILD | WS_VISIBLE | (id ? WS_TABSTOP : 0),
                x, y, width, height, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
            SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            return child;
        };
        control(L"STATIC", L"Manual fan test \u00B7 Lenovo Y700", 24, 22, 540, 28, 0);
        control(L"STATIC", L"Use while watching temperatures in your usual monitor.\r\nNormal requests Lenovo's normal mode; it is not a forced Low mode.\r\nBoost is one short dust-removal request, not sustained cooling.", 24, 62, 540, 82, 0);
        normalButton = control(L"BUTTON", L"Normal cooling", 24, 160, 170, 38, Normal);
        boostButton = control(L"BUTTON", L"Boost test (3 sec)", 204, 160, 175, 38, Boost);
        stopButton = control(L"BUTTON", L"Stop test", 390, 160, 145, 38, Stop);
        statusWindow = control(L"STATIC", L"", 24, 218, 530, 110, 0);
        status(eligible ? L"Ready. No fan commands have been sent.\r\nModel 80NV / BIOS CDCN54WW matched; fan behavior is unverified."
                        : L"Controls disabled. This test is limited to model 80NV / BIOS CDCN54WW.");
        buttons();
        return 0;
    }
    case WM_COMMAND:
        if (HIWORD(wparam) != BN_CLICKED) break;
        if (LOWORD(wparam) == Normal) start(false);
        else if (LOWORD(wparam) == Boost) start(true);
        else if (LOWORD(wparam) == Stop && active) {
            SetEvent(stopEvent);
            status(L"Stop requested. Waiting for the driver, then requesting Normal cooling.");
        }
        return 0;
    case Completed: {
        std::unique_ptr<Request> request(reinterpret_cast<Request*>(lparam));
        active = false;
        const auto& result = request->outcome;
        const unsigned finalError = request->boost ? result.restoreError : result.requestedError;
        if (finalError == 0) {
            boostTouched = false;
            status(result.requestedError == 0
                ? (request->boost ? L"Driver accepted Boost and subsequent Normal. Fan behavior is unverified.\r\nObserve both fans and temperatures; no RPM is measured."
                                  : L"Driver accepted Normal cooling. Actual fan behavior remains unverified.\r\nObserve both fans and temperatures; no RPM is measured.")
                : L"Boost failed (Windows error " + std::to_wstring(result.requestedError) +
                  L"). Driver accepted the subsequent Normal command.");
        } else {
            status(L"Normal command failed (Windows error " + std::to_wstring(finalError) +
                L"). Fan state is unknown. Retry Normal and continue monitoring.");
            closeAfter = false;
        }
        buttons();
        if (closeAfter && finalError == 0) DestroyWindow(window);
        return 0;
    }
    case WM_CLOSE:
        if (active) { closeAfter = true; SetEvent(stopEvent); status(L"Returning to Normal before closing. Waiting for the driver."); }
        else if (boostTouched) { closeAfter = true; start(false); }
        else DestroyWindow(window);
        return 0;
    case WM_POWERBROADCAST:
        if (wparam == PBT_APMSUSPEND && active) SetEvent(stopEvent);
        return TRUE;
    case WM_DESTROY:
        if (stopEvent) CloseHandle(stopEvent);
        if (font) DeleteObject(font);
        PostQuitMessage(0);
        return 0;
    default: break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    // CI startup check exits before device access or commands, including on matching hardware.
    if (std::wstring(GetCommandLineW()).find(L"--self-check") != std::wstring::npos) return 0;
    WNDCLASSW type{};
    type.hInstance = instance;
    type.lpfnWndProc = procedure;
    type.lpszClassName = L"LenovoFanCompanion.Manual";
    type.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    type.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if (!RegisterClassW(&type)) return 1;
    HWND window = CreateWindowW(type.lpszClassName, L"Lenovo Fan Companion - Manual test",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT,
        590, 390, nullptr, nullptr, instance, nullptr);
    if (!window) return 1;
    ShowWindow(window, show);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(window, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }
    }
    return 0;
}
