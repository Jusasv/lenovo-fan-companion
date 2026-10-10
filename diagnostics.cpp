// MIT; see LICENSE. Diagnostic-only: no fan commands or kernel-driver installation.
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <wbemidl.h>
#include <shlobj.h>
#include <commdlg.h>
#include <shellapi.h>
#include <algorithm>
#include <chrono>
#include <cwctype>
#include <cstring>
#include <iomanip>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
#include "diagnostic_rules.h"

namespace {
constexpr UINT ScanDone = WM_APP + 1;
constexpr int Refresh = 101, Copy = 102, Save = 103;

struct SensorResult {
    std::optional<double> cpu, gpu;
    std::wstring gpuSource, monitorStatus, nvmlStatus, nvapiStatus;
};
struct ScanResult {
    std::wstring text, laptop, driverStatus, endpointStatus;
    SensorResult sensors;
};
struct UiState {
    HWND window{}, subtitle{}, laptop{}, driver{}, cpu{}, gpu{}, report{}, refresh{}, copy{}, save{};
    HFONT titleFont{}, bodyFont{}, smallFont{}, monoFont{};
    HBRUSH background{}, white{};
    std::mutex mutex;
    ScanResult result;
    bool hasResult = false;
};
// Process lifetime: a closing window must never invalidate a background scan's memory.
UiState* ui = new UiState;

std::wstring error_code(DWORD code) {
    return L"Windows error " + std::to_wstring(code);
}
std::wstring hex_error(HRESULT code) {
    std::wostringstream out;
    out << L"0x" << std::hex << std::uppercase << static_cast<unsigned long>(code);
    return out.str();
}
std::wstring registry_string(HKEY root, const wchar_t* path, const wchar_t* name) {
    DWORD bytes = 0;
    if (RegGetValueW(root, path, name, RRF_RT_REG_SZ, nullptr, nullptr, &bytes) != ERROR_SUCCESS || bytes > 8192)
        return L"Unavailable";
    std::vector<wchar_t> buffer(bytes / sizeof(wchar_t) + 1, L'\0');
    if (RegGetValueW(root, path, name, RRF_RT_REG_SZ, nullptr, buffer.data(), &bytes) != ERROR_SUCCESS)
        return L"Unavailable";
    return buffer.data();
}
std::wstring lower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return value;
}
std::wstring device_string(HDEVINFO devices, SP_DEVINFO_DATA& device, DWORD property) {
    wchar_t buffer[2048]{};
    DWORD kind = 0;
    DWORD bytes = 0;
    if (!SetupDiGetDeviceRegistryPropertyW(devices, &device, property, &kind,
        reinterpret_cast<BYTE*>(buffer), sizeof(buffer), &bytes)) return {};
    buffer[2047] = L'\0';
    if (kind == REG_MULTI_SZ) return std::wstring(buffer, std::min<std::size_t>(bytes / sizeof(wchar_t), 2048));
    return buffer;
}
std::wstring inspect_driver(std::wostringstream& out) {
    const HDEVINFO devices = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (devices == INVALID_HANDLE_VALUE) return L"Enumeration failed: " + error_code(GetLastError());
    bool found = false;
    DWORD enumerationError = ERROR_NO_MORE_ITEMS;
    for (DWORD index = 0; index < 4096; ++index) {
        SP_DEVINFO_DATA data{};
        data.cbSize = sizeof(data);
        if (!SetupDiEnumDeviceInfo(devices, index, &data)) { enumerationError = GetLastError(); break; }
        auto name = device_string(devices, data, SPDRP_FRIENDLYNAME);
        if (name.empty()) name = device_string(devices, data, SPDRP_DEVICEDESC);
        const auto description = lower(name);
        const auto hardware = device_string(devices, data, SPDRP_HARDWAREID);
        if (description.find(L"virtual power") == std::wstring::npos && !companion::vpc_hardware_ids(hardware))
            continue;
        found = true;
        out << L"Device: " << (name.empty() ? L"Lenovo virtual power device" : name) << L"\r\n";
        ULONG status = 0, problem = 0;
        const CONFIGRET result = CM_Get_DevNode_Status(&status, &problem, data.DevInst, 0);
        if (result == CR_SUCCESS) {
            out << L"Device started: " << ((status & DN_STARTED) ? L"Yes" : L"No") << L"\r\n";
            out << L"Device problem code: " << problem << L"\r\n";
            if (problem == CM_PROB_FAILED_INSTALL) out << L"Driver installation required (Windows Code 28)\r\n";
        } else out << L"Device state: unavailable (Config Manager " << result << L")\r\n";
        const HKEY key = SetupDiOpenDevRegKey(devices, &data, DICS_FLAG_GLOBAL, 0, DIREG_DRV, KEY_READ);
        if (key != INVALID_HANDLE_VALUE) {
            out << L"Driver version: " << registry_string(key, nullptr, L"DriverVersion") << L"\r\n";
            out << L"Driver provider: " << registry_string(key, nullptr, L"ProviderName") << L"\r\n";
            out << L"Driver INF: " << registry_string(key, nullptr, L"InfPath") << L"\r\n";
            RegCloseKey(key);
        } else out << L"Driver metadata: " << error_code(GetLastError()) << L"\r\n";
    }
    SetupDiDestroyDeviceInfoList(devices);
    if (found) return L"Device found; control remains unverified";
    if (enumerationError != ERROR_NO_MORE_ITEMS) return L"Enumeration incomplete: " + error_code(enumerationError);
    return L"No matching present device found";
}

std::wstring inspect_endpoint() {
    // Opening only. Never send the reference program's mode-setting or state IOCTLs.
    HANDLE handle = CreateFileW(L"\\\\.\\EnergyDrv", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle);
        return L"Read handle opened; no fan-control commands sent";
    }
    const DWORD code = GetLastError();
    if (code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND) return L"EnergyDrv endpoint not found";
    if (code == ERROR_ACCESS_DENIED) return L"Access denied; availability cannot be established";
    if (code == ERROR_SHARING_VIOLATION) return L"Busy or sharing restricted; close other fan utilities before retrying";
    return L"Open failed: " + error_code(code);
}

std::wstring variant_string(IWbemClassObject* object, const wchar_t* name) {
    VARIANT value;
    VariantInit(&value);
    std::wstring result;
    if (SUCCEEDED(object->Get(name, 0, &value, nullptr, nullptr)) && value.vt == VT_BSTR && value.bstrVal)
        result = value.bstrVal;
    VariantClear(&value);
    return result;
}
std::optional<double> variant_number(IWbemClassObject* object, const wchar_t* name) {
    VARIANT value, number;
    VariantInit(&value);
    VariantInit(&number);
    std::optional<double> result;
    if (SUCCEEDED(object->Get(name, 0, &value, nullptr, nullptr)) && value.vt != VT_EMPTY && value.vt != VT_NULL &&
        SUCCEEDED(VariantChangeType(&number, &value, 0, VT_R8))) result = companion::temperature(number.dblVal);
    VariantClear(&number);
    VariantClear(&value);
    return result;
}
void inspect_monitor(SensorResult& sensors) {
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initialized)) { sensors.monitorStatus = L"COM unavailable: " + hex_error(initialized); return; }
    IWbemLocator* locator = nullptr;
    IWbemServices* services = nullptr;
    IEnumWbemClassObject* rows = nullptr;
    HRESULT result = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator,
        reinterpret_cast<void**>(&locator));
    if (SUCCEEDED(result)) {
        BSTR ns = SysAllocString(L"ROOT\\LibreHardwareMonitor");
        if (!ns) result = E_OUTOFMEMORY;
        else {
            result = locator->ConnectServer(ns, nullptr, nullptr, nullptr, WBEM_FLAG_CONNECT_USE_MAX_WAIT,
                nullptr, nullptr, &services);
            SysFreeString(ns);
        }
    }
    if (SUCCEEDED(result)) result = CoSetProxyBlanket(services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
        nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
    if (SUCCEEDED(result)) {
        BSTR language = SysAllocString(L"WQL");
        BSTR query = SysAllocString(L"SELECT Identifier, Value FROM Sensor WHERE SensorType = 'Temperature'");
        if (!language || !query) result = E_OUTOFMEMORY;
        else result = services->ExecQuery(language, query, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            nullptr, &rows);
        SysFreeString(language);
        SysFreeString(query);
    }
    if (SUCCEEDED(result)) {
        sensors.monitorStatus = L"Existing provider queried; reading freshness is not verified";
        const auto start = std::chrono::steady_clock::now();
        for (unsigned index = 0; index < 128; ++index) {
            IWbemClassObject* row = nullptr;
            ULONG count = 0;
            result = rows->Next(250, 1, &row, &count);
            if (FAILED(result)) { sensors.monitorStatus = L"Provider enumeration failed: " + hex_error(result); break; }
            if (!count) {
                if (result == WBEM_S_TIMEDOUT) sensors.monitorStatus = L"Provider timed out; partial data only";
                break;
            }
            const auto value = variant_number(row, L"Value");
            const auto kind = companion::sensor_kind(variant_string(row, L"Identifier"));
            if (value && kind == companion::SensorKind::Cpu) companion::take_hottest(sensors.cpu, *value);
            if (value && kind == companion::SensorKind::Gpu) companion::take_hottest(sensors.gpu, *value);
            row->Release();
            if (std::chrono::steady_clock::now() - start > std::chrono::seconds(2)) {
                sensors.monitorStatus = L"Enumeration budget reached; partial data only";
                break;
            }
        }
    } else if (result == static_cast<HRESULT>(WBEM_E_INVALID_NAMESPACE) || result == static_cast<HRESULT>(WBEM_E_INVALID_CLASS))
        sensors.monitorStatus = L"No existing LibreHardwareMonitor temperature provider";
    else sensors.monitorStatus = L"Provider unavailable: " + hex_error(result);
    if (sensors.gpu) sensors.gpuSource = L"Existing LibreHardwareMonitor provider";
    if (rows) rows->Release();
    if (services) services->Release();
    if (locator) locator->Release();
    CoUninitialize();
}

template<class Function> Function library_function(HMODULE library, const char* name) {
    const FARPROC address = GetProcAddress(library, name);
    static_assert(sizeof(Function) == sizeof(address));
    Function function = nullptr;
    std::memcpy(&function, &address, sizeof(function));
    return function;
}

void inspect_nvml(SensorResult& sensors) {
    wchar_t system[MAX_PATH]{};
    HMODULE library = nullptr;
    if (GetSystemDirectoryW(system, MAX_PATH)) {
        const auto path = std::wstring(system) + L"\\nvml.dll";
        library = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    }
    if (!library) {
        PWSTR programFiles = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramFiles, 0, nullptr, &programFiles))) {
            const auto path = std::wstring(programFiles) + L"\\NVIDIA Corporation\\NVSMI\\nvml.dll";
            library = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
            CoTaskMemFree(programFiles);
        }
    }
    if (!library) { sensors.nvmlStatus = L"Installed NVML library not found in trusted vendor locations"; return; }
    // Official NVIDIA NVML C ABI; no vendor library is bundled or downloaded.
    using Init = int(__cdecl*)();
    using Count = int(__cdecl*)(unsigned*);
    using GetDevice = int(__cdecl*)(unsigned, void**);
    using GetTemperature = int(__cdecl*)(void*, unsigned, unsigned*);
    const auto init = library_function<Init>(library, "nvmlInit_v2");
    const auto shutdown = library_function<Init>(library, "nvmlShutdown");
    const auto count = library_function<Count>(library, "nvmlDeviceGetCount_v2");
    const auto device = library_function<GetDevice>(library, "nvmlDeviceGetHandleByIndex_v2");
    const auto temperature = library_function<GetTemperature>(library, "nvmlDeviceGetTemperature");
    if (!init || !shutdown || !count || !device || !temperature) {
        sensors.nvmlStatus = L"Installed NVML does not expose the required temperature interface";
        FreeLibrary(library);
        return;
    }
    const int initialized = init();
    if (initialized != 0) {
        sensors.nvmlStatus = L"NVML initialization failed (code " + std::to_wstring(initialized) + L")";
        FreeLibrary(library);
        return;
    }
    unsigned total = 0;
    const int enumeration = count(&total);
    std::optional<double> hottest;
    if (enumeration == 0) {
        for (unsigned i = 0; i < (std::min)(total, 16u); ++i) {
            void* handle = nullptr;
            unsigned value = 0;
            if (device(i, &handle) == 0 && temperature(handle, 0, &value) == 0)
                companion::take_hottest(hottest, static_cast<double>(value));
        }
    }
    shutdown();
    FreeLibrary(library);
    if (hottest) {
        sensors.gpu = hottest;
        sensors.gpuSource = L"NVIDIA NVML (hottest accessible GPU)";
        sensors.nvmlStatus = L"Temperature read successfully";
    } else sensors.nvmlStatus = enumeration == 0 ? L"No accessible GPU temperature (device asleep or unsupported is possible)" :
        L"NVML device enumeration failed (code " + std::to_wstring(enumeration) + L")";
}

void inspect_nvapi(SensorResult& sensors) {
    if (sensors.gpu && sensors.gpuSource.find(L"NVML") != std::wstring::npos) {
        sensors.nvapiStatus = L"Not queried; NVML provided GPU temperature";
        return;
    }
    wchar_t system[MAX_PATH]{};
    if (!GetSystemDirectoryW(system, MAX_PATH)) {
        sensors.nvapiStatus = L"Windows system directory unavailable";
        return;
    }
    const auto path = std::wstring(system) + L"\\nvapi64.dll";
    HMODULE library = LoadLibraryExW(path.c_str(), nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!library) {
        sensors.nvapiStatus = L"Installed NVIDIA NVAPI library unavailable";
        return;
    }
    // Documented NVIDIA NVAPI V2 thermal ABI. Query only; no cooling setters.
    struct ThermalSensor { int controller, minimum, maximum, current, target; };
    struct ThermalSettings { unsigned version, count; ThermalSensor sensor[3]; };
    static_assert(sizeof(ThermalSettings) == 68, "NVIDIA thermal ABI size");
    using Query = void* (__cdecl*)(unsigned);
    using Initialize = int(__cdecl*)();
    using Enumerate = int(__cdecl*)(void**, unsigned*);
    using ReadThermal = int(__cdecl*)(void*, unsigned, ThermalSettings*);
    const auto query = library_function<Query>(library, "nvapi_QueryInterface");
    Initialize initialize = nullptr, unload = nullptr;
    Enumerate enumerate = nullptr;
    ReadThermal thermal = nullptr;
    // Copy exported pointers into typed function pointers, matching library_function.
    auto resolve = [&](auto& target, unsigned identifier) {
        if (query) { const auto address = query(identifier); std::memcpy(&target, &address, sizeof(target)); }
    };
    resolve(initialize, 0x0150e828u);
    resolve(unload, 0xd22bdd7eu);
    resolve(enumerate, 0xe5ac921fu);
    resolve(thermal, 0xe3640a56u);
    if (!initialize || !unload || !enumerate || !thermal) {
        sensors.nvapiStatus = L"Required NVIDIA NVAPI interface unavailable";
        FreeLibrary(library);
        return;
    }
    const int initialized = initialize();
    if (initialized != 0) {
        sensors.nvapiStatus = L"NVAPI initialization failed (code " + std::to_wstring(initialized) + L")";
        FreeLibrary(library);
        return;
    }
    void* handles[64]{};
    unsigned count = 0;
    const int enumerated = enumerate(handles, &count);
    std::optional<double> hottest;
    if (enumerated == 0) {
        for (unsigned index = 0; index < (std::min)(count, 64u); ++index) {
            ThermalSettings settings{};
            settings.version = static_cast<unsigned>(sizeof(settings)) | (2u << 16);
            if (thermal(handles[index], 15u, &settings) != 0) continue;
            for (unsigned sensor = 0; sensor < (std::min)(settings.count, 3u); ++sensor) {
                // GPU core only: board, memory and power temperatures are distinct.
                if (settings.sensor[sensor].target == 1)
                    companion::take_hottest(hottest, settings.sensor[sensor].current);
            }
        }
    }
    unload();
    FreeLibrary(library);
    if (hottest) {
        sensors.gpu = hottest;
        sensors.gpuSource = L"NVIDIA NVAPI (hottest accessible GPU core)";
        sensors.nvapiStatus = L"Temperature read successfully";
    } else sensors.nvapiStatus = enumerated == 0 ? L"No valid GPU core temperature returned" :
        L"NVAPI device enumeration failed (code " + std::to_wstring(enumerated) + L")";
}

std::wstring temperature_text(const std::optional<double>& value) {
    if (!value) return L"Unavailable";
    std::wostringstream out;
    out << std::fixed << std::setprecision(1) << *value << L" \u00B0C";
    return out.str();
}
ScanResult collect() {
    ScanResult result;
    std::wostringstream out, driver;
    constexpr auto bios = L"HARDWARE\\DESCRIPTION\\System\\BIOS";
    const auto maker = registry_string(HKEY_LOCAL_MACHINE, bios, L"SystemManufacturer");
    const auto model = registry_string(HKEY_LOCAL_MACHINE, bios, L"SystemProductName");
    result.laptop = maker + L" " + model;
    const auto firmware = registry_string(HKEY_LOCAL_MACHINE, bios, L"BIOSVersion");
    constexpr auto windows = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
    const auto build = registry_string(HKEY_LOCAL_MACHINE, windows, L"CurrentBuildNumber");
    const auto release = registry_string(HKEY_LOCAL_MACHINE, windows, L"DisplayVersion");
    const auto product = registry_string(HKEY_LOCAL_MACHINE, windows, L"ProductName");
    unsigned buildNumber = 0;
    try { buildNumber = static_cast<unsigned>(std::stoul(build)); } catch (...) {}
    const auto os = buildNumber >= 22000 ? L"Windows 11" : (buildNumber >= 10240 ? L"Windows 10" : L"Windows (version unverified)");
    SYSTEMTIME now{};
    GetSystemTime(&now);
    out << L"LENOVO FAN COMPANION | Diagnostics 0.1\r\n"
        << L"Read-only scan. No fan commands were sent.\r\n\r\n"
        << L"Scan time (UTC): " << now.wYear << L"-" << std::setw(2) << std::setfill(L'0') << now.wMonth
        << L"-" << std::setw(2) << now.wDay << L" " << std::setw(2) << now.wHour << L":" << std::setw(2) << now.wMinute
        << L":" << std::setw(2) << now.wSecond << L"\r\n\r\n"
        << L"LAPTOP\r\nManufacturer: " << maker << L"\r\nModel: " << model << L"\r\nBIOS: " << firmware
        << L"\r\nOS: " << os << L" " << release << L" (build " << build << L")\r\n"
        << L"Registry product label (may be outdated): " << product << L"\r\n\r\n";
    result.driverStatus = inspect_driver(driver);
    result.endpointStatus = inspect_endpoint();
    out << L"LENOVO DRIVER\r\n" << result.driverStatus << L"\r\n" << driver.str()
        << L"Endpoint: " << result.endpointStatus << L"\r\nFan-control support: unverified\r\n\r\n";
    inspect_monitor(result.sensors);
    inspect_nvml(result.sensors);
    inspect_nvapi(result.sensors);
    out << L"TEMPERATURE SNAPSHOT\r\nCPU: " << temperature_text(result.sensors.cpu)
        << L"\r\nCPU source: " << (result.sensors.cpu ? L"Existing LibreHardwareMonitor provider (hottest CPU sensor)" : L"None available")
        << L"\r\nGPU: " << temperature_text(result.sensors.gpu)
        << L"\r\nGPU source: " << (result.sensors.gpuSource.empty() ? L"None available" : result.sensors.gpuSource)
        << L"\r\nMonitor provider: " << result.sensors.monitorStatus
        << L"\r\nNVIDIA NVML provider: " << result.sensors.nvmlStatus
        << L"\r\nNVIDIA NVAPI provider: " << result.sensors.nvapiStatus
        << L"\r\nSnapshot only; existing monitor freshness is not verified.\r\n"
        << L"ACPI thermal zones are not treated as CPU temperature.\r\n\r\n"
        << L"COOLING\r\nQuiet / Balanced / Gaming profiles: not implemented in this diagnostic build.\r\n"
        << L"Sustained Boost: unverified; reference High mode reportedly cycles on the Y700.\r\n"
        << L"No RPM or fan percentage is measured by this build.\r\n\r\n"
        << L"SHARING\r\nThis report omits serial numbers, user names, machine names, and full device IDs.\r\n"
        << L"Review the report before sharing. Nothing is uploaded automatically.\r\n";
    result.text = out.str();
    return result;
}

bool save_report(const std::wstring& path, const std::wstring& report) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    const auto bytes = companion::utf8(report);
    DWORD written = 0;
    const bool success = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) && written == bytes.size();
    CloseHandle(file);
    return success;
}
DWORD WINAPI scan_worker(void*) {
    ScanResult result;
    try { result = collect(); }
    catch (...) { result.text = L"Diagnostic scan failed unexpectedly. No fan commands were sent.\r\n"; result.driverStatus = L"Scan failed"; }
    {
        std::lock_guard<std::mutex> lock(ui->mutex);
        ui->result = std::move(result);
    }
    PostMessageW(ui->window, ScanDone, 0, 0);
    return 0;
}
void start_scan() {
    EnableWindow(ui->refresh, FALSE);
    EnableWindow(ui->copy, FALSE);
    EnableWindow(ui->save, FALSE);
    SetWindowTextW(ui->subtitle, L"Checking local driver and sensor availability\u2026");
    SetWindowTextW(ui->cpu, L"CPU\r\nChecking\u2026");
    SetWindowTextW(ui->gpu, L"GPU\r\nChecking\u2026");
    HANDLE worker = CreateThread(nullptr, 0, scan_worker, nullptr, 0, nullptr);
    if (worker) CloseHandle(worker);
    else {
        SetWindowTextW(ui->subtitle, L"Unable to start scan. No fan commands were sent.");
        EnableWindow(ui->refresh, TRUE);
    }
}
int scaled(HWND window, int value) {
    const UINT dpi = GetDpiForWindow(window);
    return MulDiv(value, static_cast<int>(dpi), 96);
}
HFONT make_font(HWND window, int size, int weight, const wchar_t* face = L"Segoe UI") {
    return CreateFontW(-scaled(window, size), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, face);
}
HWND control(HWND parent, const wchar_t* kind, const wchar_t* text, DWORD styles, int id, HFONT font) {
    HWND result = CreateWindowExW(0, kind, text, WS_CHILD | WS_VISIBLE | styles, 0, 0, 1, 1, parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
    SendMessageW(result, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    return result;
}
void layout(HWND window) {
    RECT client{};
    GetClientRect(window, &client);
    const int padding = scaled(window, 28), gap = scaled(window, 12);
    const int width = client.right - padding * 2;
    MoveWindow(GetDlgItem(window, 201), padding, scaled(window, 24), width, scaled(window, 36), TRUE);
    MoveWindow(ui->subtitle, padding, scaled(window, 68), width, scaled(window, 26), TRUE);
    MoveWindow(ui->laptop, padding, scaled(window, 116), width - scaled(window, 216), scaled(window, 44), TRUE);
    MoveWindow(ui->cpu, client.right - padding - scaled(window, 200), scaled(window, 116), scaled(window, 96), scaled(window, 54), TRUE);
    MoveWindow(ui->gpu, client.right - padding - scaled(window, 92), scaled(window, 116), scaled(window, 92), scaled(window, 54), TRUE);
    MoveWindow(ui->driver, padding, scaled(window, 176), width, scaled(window, 44), TRUE);
    const int bottom = client.bottom - scaled(window, 60);
    MoveWindow(ui->report, padding, scaled(window, 232), width, (std::max)(scaled(window, 100), bottom - scaled(window, 246)), TRUE);
    const int button = scaled(window, 128);
    MoveWindow(ui->refresh, padding, bottom, button, scaled(window, 34), TRUE);
    MoveWindow(ui->copy, padding + button + gap, bottom, button, scaled(window, 34), TRUE);
    MoveWindow(ui->save, padding + (button + gap) * 2, bottom, button, scaled(window, 34), TRUE);
}
void copy_report(HWND window) {
    if (!OpenClipboard(window)) return;
    const auto& text = ui->result.text;
    HGLOBAL storage = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t));
    if (storage) {
        void* memory = GlobalLock(storage);
        if (memory) {
            memcpy(memory, text.c_str(), (text.size() + 1) * sizeof(wchar_t));
            GlobalUnlock(storage);
            if (EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, storage)) storage = nullptr;
        }
        if (storage) GlobalFree(storage);
    }
    CloseClipboard();
}
LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE: {
        ui->window = window;
        ui->background = CreateSolidBrush(RGB(246, 248, 250));
        ui->white = CreateSolidBrush(RGB(255, 255, 255));
        ui->titleFont = make_font(window, 25, FW_SEMIBOLD);
        ui->bodyFont = make_font(window, 15, FW_NORMAL);
        ui->smallFont = make_font(window, 13, FW_NORMAL);
        ui->monoFont = make_font(window, 13, FW_NORMAL, L"Consolas");
        control(window, L"STATIC", L"Lenovo Fan Companion", SS_LEFT, 201, ui->titleFont);
        ui->subtitle = control(window, L"STATIC", L"Diagnostics preview 0.1 \u00B7 Read-only", SS_LEFT, 202, ui->smallFont);
        ui->laptop = control(window, L"STATIC", L"LAPTOP\r\nChecking\u2026", SS_LEFT, 203, ui->bodyFont);
        ui->cpu = control(window, L"STATIC", L"CPU\r\nUnavailable", SS_LEFT, 204, ui->bodyFont);
        ui->gpu = control(window, L"STATIC", L"GPU\r\nUnavailable", SS_LEFT, 205, ui->bodyFont);
        ui->driver = control(window, L"STATIC", L"LENOVO DRIVER\r\nChecking\u2026", SS_LEFT, 206, ui->smallFont);
        ui->report = control(window, L"EDIT", L"Starting read-only diagnostic scan\u2026", ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL | WS_TABSTOP, 207, ui->monoFont);
        SendMessageW(ui->report, EM_SETLIMITTEXT, 65536, 0);
        ui->refresh = control(window, L"BUTTON", L"Refresh", BS_PUSHBUTTON | WS_TABSTOP, Refresh, ui->smallFont);
        ui->copy = control(window, L"BUTTON", L"Copy report", BS_PUSHBUTTON | WS_TABSTOP, Copy, ui->smallFont);
        ui->save = control(window, L"BUTTON", L"Save report\u2026", BS_PUSHBUTTON | WS_TABSTOP, Save, ui->smallFont);
        layout(window);
        start_scan();
        return 0;
    }
    case ScanDone: {
        std::lock_guard<std::mutex> lock(ui->mutex);
        ui->hasResult = true;
        SetWindowTextW(ui->subtitle, L"Diagnostics preview 0.1 \u00B7 No fan settings changed \u00B7 Snapshot readings");
        SetWindowTextW(ui->laptop, (L"LAPTOP\r\n" + ui->result.laptop).c_str());
        SetWindowTextW(ui->driver, (L"LENOVO DRIVER\r\n" + ui->result.driverStatus).c_str());
        SetWindowTextW(ui->cpu, (L"CPU\r\n" + temperature_text(ui->result.sensors.cpu)).c_str());
        SetWindowTextW(ui->gpu, (L"GPU\r\n" + temperature_text(ui->result.sensors.gpu)).c_str());
        SetWindowTextW(ui->report, ui->result.text.c_str());
        EnableWindow(ui->refresh, TRUE);
        EnableWindow(ui->copy, TRUE);
        EnableWindow(ui->save, TRUE);
        return 0;
    }
    case WM_SIZE: layout(window); return 0;
    case WM_GETMINMAXINFO: {
        auto* size = reinterpret_cast<MINMAXINFO*>(lparam);
        size->ptMinTrackSize.x = scaled(window, 700);
        size->ptMinTrackSize.y = scaled(window, 560);
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HDC context = reinterpret_cast<HDC>(wparam);
        const bool report = reinterpret_cast<HWND>(lparam) == ui->report;
        SetTextColor(context, RGB(32, 48, 60));
        SetBkColor(context, report ? RGB(255, 255, 255) : RGB(246, 248, 250));
        return reinterpret_cast<LRESULT>(report ? ui->white : ui->background);
    }
    case WM_ERASEBKGND: {
        RECT client{};
        GetClientRect(window, &client);
        FillRect(reinterpret_cast<HDC>(wparam), &client, ui->background);
        return 1;
    }
    case WM_COMMAND:
        if (HIWORD(wparam) != BN_CLICKED) break;
        if (LOWORD(wparam) == Refresh) start_scan();
        else if (LOWORD(wparam) == Copy && ui->hasResult) copy_report(window);
        else if (LOWORD(wparam) == Save && ui->hasResult) {
            wchar_t path[32768] = L"fan-companion-diagnostics.txt";
            OPENFILENAMEW dialog{};
            dialog.lStructSize = sizeof(dialog);
            dialog.hwndOwner = window;
            dialog.lpstrFilter = L"Text report (*.txt)\0*.txt\0\0";
            dialog.lpstrFile = path;
            dialog.nMaxFile = 32768;
            dialog.lpstrDefExt = L"txt";
            dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
            if (GetSaveFileNameW(&dialog) && !save_report(path, ui->result.text))
                MessageBoxW(window, L"The report could not be saved.", L"Save report", MB_OK | MB_ICONERROR);
        }
        return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    default: break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const HRESULT security = CoInitializeSecurity(nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);
    (void)security; // The worker reports query failures, including security configuration errors.
    int count = 0;
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (arguments && count == 3 && std::wstring(arguments[1]) == L"--report") {
        bool success = false;
        try { success = save_report(arguments[2], collect().text); } catch (...) {}
        LocalFree(arguments);
        if (SUCCEEDED(com)) CoUninitialize();
        return success ? 0 : 1;
    }
    if (arguments) LocalFree(arguments);
    WNDCLASSW type{};
    type.lpfnWndProc = window_proc;
    type.hInstance = instance;
    type.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    type.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    type.lpszClassName = L"LenovoFanCompanion.Diagnostics";
    if (!RegisterClassW(&type)) return 1;
    HWND window = CreateWindowExW(0, type.lpszClassName, L"Lenovo Fan Companion \u2014 Diagnostics",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, 860, 740,
        nullptr, nullptr, instance, nullptr);
    if (!window) return 1;
    ShowWindow(window, show);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(window, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }
    }
    if (SUCCEEDED(com)) CoUninitialize();
    return 0;
}
