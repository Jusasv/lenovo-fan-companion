// MIT; see LICENSE and THIRD_PARTY_NOTICES.md. Experimental manual hardware test.
#include <windows.h>
#include <shellapi.h>
#include <cmath>
#include <cwchar>
#include <atomic>
#include <string>
#include <memory>
#include "manual_rules.h"
namespace {
constexpr int Normal = 101, Boost = 102, Stop = 103, Open = 104, Exit = 105, Hide = 106;
constexpr UINT Completed = WM_APP + 1, TrayEvent = WM_APP + 2;
HWND mainWindow{}, statusWindow{}, normalButton{}, boostButton{}, stopButton{};
HANDLE stopEvent{};
bool active = false, closeAfter = false, eligible = false, boostTouched = false;
HFONT font{}, headingFont{};
HICON appIcon{};
NOTIFYICONDATAW tray{};
bool trayAvailable = false;
UINT taskbarCreated{};
std::wstring lastStatus;
bool smokeTest = false;
std::atomic_uint commandAttempts{0};
constexpr wchar_t WindowClass[] = L"LenovoFanCompanion.Manual";
HICON make_icon() {
    BITMAPINFO bitmap{};
    bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap.bmiHeader.biWidth = 32;
    bitmap.bmiHeader.biHeight = -32;
    bitmap.bmiHeader.biPlanes = 1;
    bitmap.bmiHeader.biBitCount = 32;
    bitmap.bmiHeader.biCompression = BI_RGB;
    void* memory = nullptr;
    HBITMAP color = CreateDIBSection(nullptr, &bitmap, DIB_RGB_COLORS, &memory, nullptr, 0);
    if (!color || !memory) { if (color) DeleteObject(color); return nullptr; }
    auto* pixels = static_cast<DWORD*>(memory);
    for (int y = 0; y < 32; ++y) for (int x = 0; x < 32; ++x) {
        const double dx = x - 15.5, dy = y - 15.5;
        bool blade = dx * dx + dy * dy < 7;
        for (int part = 0; part < 3; ++part) {
            const double angle = part * 2.0943951024;
            const double u = dx * std::cos(angle) + dy * std::sin(angle) - 6;
            const double v = -dx * std::sin(angle) + dy * std::cos(angle);
            if (u * u / 25 + v * v / 10 < 1) blade = true;
        }
        pixels[y * 32 + x] = dx * dx + dy * dy > 235 ? 0u : (blade ? 0xfff5fbffu : 0xff176b78u);
    }
    const BYTE maskBits[128]{};
    HBITMAP mask = CreateBitmap(32, 32, 1, 1, maskBits);
    ICONINFO info{};
    info.fIcon = TRUE; info.hbmColor = color; info.hbmMask = mask;
    HICON icon = mask ? CreateIconIndirect(&info) : nullptr;
    if (mask) DeleteObject(mask);
    DeleteObject(color);
    return icon;
}
void show_window() {
    ShowWindow(mainWindow, SW_RESTORE);
    SetForegroundWindow(mainWindow);
}
void update_tip() {
    const auto end = lastStatus.find_first_of(L"\r\n");
    const auto tip = L"Lenovo Fan Companion: " + lastStatus.substr(0, end);
    wcsncpy_s(tray.szTip, tip.c_str(), _TRUNCATE);
    if (trayAvailable) Shell_NotifyIconW(NIM_MODIFY, &tray);
}
bool add_tray() {
    tray.cbSize = sizeof(tray); tray.hWnd = mainWindow; tray.uID = 1;
    tray.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    tray.uCallbackMessage = TrayEvent;
    tray.hIcon = appIcon ? appIcon : LoadIconW(nullptr, IDI_APPLICATION);
    update_tip();
    trayAvailable = Shell_NotifyIconW(NIM_ADD, &tray) != FALSE;
    if (trayAvailable) { tray.uVersion = NOTIFYICON_VERSION_4; Shell_NotifyIconW(NIM_SETVERSION, &tray); }
    return trayAvailable;
}
void tray_menu() {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    const UINT actions = eligible && !active ? MF_ENABLED : MF_GRAYED;
    AppendMenuW(menu, MF_STRING, Open, L"Open Lenovo Fan Companion");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | actions, Normal, L"Normal cooling");
    AppendMenuW(menu, MF_STRING | actions, Boost, L"Short Boost test (3 sec)");
    AppendMenuW(menu, MF_STRING | (active ? MF_ENABLED : MF_GRAYED), Stop, L"Stop test / request Normal");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, Exit, L"Exit");
    SetMenuDefaultItem(menu, Open, FALSE);
    POINT position{};
    GetCursorPos(&position);
    SetForegroundWindow(mainWindow);
    const auto action = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
        position.x, position.y, 0, mainWindow, nullptr);
    DestroyMenu(menu);
    if (action) PostMessageW(mainWindow, WM_COMMAND, action, 0);
    PostMessageW(mainWindow, WM_NULL, 0, 0);
    Shell_NotifyIconW(NIM_SETFOCUS, &tray);
}
std::wstring registry_text(const wchar_t* key) {
    wchar_t value[256]{};
    DWORD size = sizeof(value);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\BIOS", key,
        RRF_RT_REG_SZ, nullptr, value, &size) != ERROR_SUCCESS) return {};
    return value;
}
unsigned send_mode(unsigned mode) {
    ++commandAttempts;
    if (smokeTest) return ERROR_ACCESS_DENIED; // UI checks must never access the device.
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
void status(const std::wstring& text) { lastStatus = text; SetWindowTextW(statusWindow, text.c_str()); update_tip(); }
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
    if (taskbarCreated && message == taskbarCreated) {
        trayAvailable = false;
        if (!add_tray()) show_window();
        return 0;
    }
    switch (message) {
    case WM_CREATE: {
        mainWindow = window;
        stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        eligible = stopEvent && companion::manual_target(registry_text(L"SystemProductName"), registry_text(L"BIOSVersion"));
        const UINT dpi = GetDpiForWindow(window);
        auto scale = [dpi](int value) { return MulDiv(value, static_cast<int>(dpi), 96); };
        font = CreateFontW(-scale(15), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        headingFont = CreateFontW(-scale(25), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        auto control = [&](const wchar_t* type, const wchar_t* text, int x, int y, int width, int height, int id) {
            HWND child = CreateWindowW(type, text, WS_CHILD | WS_VISIBLE | (id ? WS_TABSTOP : 0),
                scale(x), scale(y), scale(width), scale(height), window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
            SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            return child;
        };
        HWND heading = control(L"STATIC", L"Lenovo Fan Companion", 24, 22, 540, 36, 0);
        SendMessageW(heading, WM_SETFONT, reinterpret_cast<WPARAM>(headingFont), TRUE);
        control(L"STATIC", L"Manual cooling \u00B7 Y700 \u00B7 Preview 0.2", 24, 66, 530, 24, 0);
        control(L"STATIC", L"Request normal cooling or try a short Boost test.\r\nKeep your usual temperature monitor open while testing.", 24, 104, 530, 46, 0);
        normalButton = control(L"BUTTON", L"Normal cooling", 24, 160, 170, 38, Normal);
        boostButton = control(L"BUTTON", L"Boost test (3 sec)", 204, 160, 175, 38, Boost);
        stopButton = control(L"BUTTON", L"Stop test", 390, 160, 145, 38, Stop);
        statusWindow = control(L"STATIC", L"", 24, 218, 530, 94, 0);
        control(L"BUTTON", L"Hide to tray", 24, 322, 170, 34, Hide);
        control(L"BUTTON", L"Exit", 204, 322, 100, 34, Exit);
        control(L"STATIC", L"Close hides to tray. Exit stops the app.", 24, 368, 530, 22, 0);
        status(eligible ? L"Ready. No fan commands have been sent.\r\nModel 80NV / BIOS CDCN54WW matched; fan behavior is unverified."
                        : L"Controls disabled. This test is limited to model 80NV / BIOS CDCN54WW.");
        buttons();
        taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
        if (!add_tray()) status(L"Tray unavailable. The window will stay accessible; Close exits the app.");
        return 0;
    }
    case WM_COMMAND:
        if (HIWORD(wparam) != BN_CLICKED) break;
        if (LOWORD(wparam) == Open) show_window();
        else if (LOWORD(wparam) == Hide) {
            if (trayAvailable) ShowWindow(window, SW_HIDE);
            else status(L"Tray unavailable. Keep this window open or use Exit.");
        }
        else if (LOWORD(wparam) == Exit) SendMessageW(window, WM_APP + 3, 0, 0);
        else if (LOWORD(wparam) == Normal) start(false);
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
            show_window();
        }
        buttons();
        if (closeAfter && finalError == 0) DestroyWindow(window);
        return 0;
    }
    case WM_CLOSE:
        if (trayAvailable) { ShowWindow(window, SW_HIDE); return 0; }
        [[fallthrough]];
    case WM_APP + 3:
        if (active) { closeAfter = true; SetEvent(stopEvent); status(L"Returning to Normal before closing. Waiting for the driver."); }
        else if (boostTouched) { closeAfter = true; start(false); }
        else DestroyWindow(window);
        return 0;
    case TrayEvent:
        if (LOWORD(lparam) == WM_CONTEXTMENU || LOWORD(lparam) == WM_RBUTTONUP) tray_menu();
        else if (LOWORD(lparam) == NIN_SELECT || LOWORD(lparam) == NIN_KEYSELECT || LOWORD(lparam) == WM_LBUTTONDBLCLK || LOWORD(lparam) == WM_LBUTTONUP)
            show_window();
        return 0;
    case WM_CTLCOLORSTATIC: {
        HDC context = reinterpret_cast<HDC>(wparam);
        SetTextColor(context, RGB(30, 48, 58));
        SetBkColor(context, RGB(246, 248, 250));
        SetDCBrushColor(context, RGB(246, 248, 250));
        return reinterpret_cast<LRESULT>(GetStockObject(DC_BRUSH));
    }
    case WM_ERASEBKGND: {
        RECT area{}; GetClientRect(window, &area);
        HDC context = reinterpret_cast<HDC>(wparam);
        SetDCBrushColor(context, RGB(246, 248, 250));
        FillRect(context, &area, reinterpret_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
        return 1;
    }
    case WM_TIMER:
        if (smokeTest && wparam == 1) {
            KillTimer(window, 1);
            bool passed = IsWindowVisible(window) != FALSE;
            if (trayAvailable) {
                SendMessageW(window, WM_CLOSE, 0, 0);
                passed = passed && !IsWindowVisible(window);
                SendMessageW(window, WM_COMMAND, Open, 0);
                passed = passed && IsWindowVisible(window);
            }
            const bool hadTray = trayAvailable;
            SendMessageW(window, WM_COMMAND, Exit, 0);
            // 3 means no Explorer tray on this desktop; fallback Exit still exercised.
            PostQuitMessage(passed && commandAttempts == 0 ? (hadTray ? 0 : 3) : 1);
        }
        return 0;
    case WM_POWERBROADCAST:
        if (wparam == PBT_APMSUSPEND && active) SetEvent(stopEvent);
        return TRUE;
    case WM_DESTROY:
        if (trayAvailable) Shell_NotifyIconW(NIM_DELETE, &tray);
        if (stopEvent) CloseHandle(stopEvent);
        if (font) DeleteObject(font);
        if (headingFont) DeleteObject(headingFont);
        PostQuitMessage(0);
        return 0;
    default: break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    // CI startup check exits before device access or commands, including on matching hardware.
    int argumentCount = 0;
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    if (!arguments) return 1;
    const bool selfCheck = argumentCount == 2 && std::wstring(arguments[1]) == L"--self-check";
    smokeTest = argumentCount == 2 && std::wstring(arguments[1]) == L"--ui-smoke-test";
    const bool validArguments = argumentCount == 1 || selfCheck || smokeTest;
    LocalFree(arguments);
    if (!validArguments) return 1;
    if (selfCheck) return 0;
    HANDLE single = CreateMutexW(nullptr, FALSE, L"Local\\LenovoFanCompanion.Manual");
    if (!single) return 1;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(WindowClass, nullptr);
        if (existing) { ShowWindow(existing, SW_RESTORE); SetForegroundWindow(existing); }
        CloseHandle(single);
        return smokeTest ? 2 : 0;
    }
    // The preceding manual preview predates the mutex. Do not run beside it.
    if (HWND existing = FindWindowW(WindowClass, nullptr)) {
        ShowWindow(existing, SW_RESTORE); SetForegroundWindow(existing);
        MessageBoxW(existing, L"Close the earlier manual preview before opening this version.", L"Lenovo Fan Companion", MB_OK);
        CloseHandle(single); return 0;
    }
    appIcon = make_icon();
    WNDCLASSW type{};
    type.hInstance = instance;
    type.lpfnWndProc = procedure;
    type.lpszClassName = WindowClass;
    type.hIcon = appIcon;
    type.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    type.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if (!RegisterClassW(&type)) return 1;
    const int dpi = static_cast<int>(GetDpiForSystem());
    HWND window = CreateWindowW(type.lpszClassName, L"Lenovo Fan Companion",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT,
        MulDiv(590, dpi, 96), MulDiv(440, dpi, 96), nullptr, nullptr, instance, nullptr);
    if (!window) return 1;
    ShowWindow(window, show);
    if (smokeTest && !SetTimer(window, 1, 150, nullptr)) { DestroyWindow(window); return 1; }
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(window, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }
    }
    if (appIcon) DestroyIcon(appIcon);
    CloseHandle(single);
    return static_cast<int>(message.wParam);
}
