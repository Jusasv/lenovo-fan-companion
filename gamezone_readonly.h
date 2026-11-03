#pragma once
#include <windows.h>
#include <wbemidl.h>
#include <optional>
#include <cwchar>
#include <string>
#include "diagnostic_rules.h"
#include "manual_rules.h"

namespace companion {
namespace gamezone_detail {
template<class T> struct Pointer {
    T* value = nullptr;
    Pointer() = default;
    Pointer(const Pointer&) = delete;
    Pointer& operator=(const Pointer&) = delete;
    ~Pointer() { if (value) value->Release(); }
    T** receive() { return &value; }
    T* operator->() const { return value; }
};
struct Text {
    BSTR value;
    explicit Text(const wchar_t* text) : value(SysAllocString(text)) {}
    ~Text() { SysFreeString(value); }
    Text(const Text&) = delete;
    Text& operator=(const Text&) = delete;
};
struct Apartment {
    HRESULT status = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ~Apartment() { if (SUCCEEDED(status)) CoUninitialize(); }
};
inline std::optional<unsigned> number(IWbemClassObject* object, const wchar_t* key) {
    VARIANT value; VariantInit(&value);
    std::optional<unsigned> result;
    if (SUCCEEDED(object->Get(key, 0, &value, nullptr, nullptr))) {
        if (value.vt == VT_UI4) result = value.ulVal;
        else if (value.vt == VT_I4 && value.lVal >= 0) result = static_cast<unsigned>(value.lVal);
    }
    VariantClear(&value);
    return result;
}
inline bool method_matches(IWbemClassObject* definition, const wchar_t* name, unsigned expected) {
    Pointer<IWbemQualifierSet> qualifiers;
    if (FAILED(definition->GetMethodQualifierSet(name, qualifiers.receive()))) return false;
    VARIANT value; VariantInit(&value);
    const HRESULT status = qualifiers->Get(L"WmiMethodId", 0, &value, nullptr);
    const bool matches = SUCCEEDED(status) &&
        ((value.vt == VT_I4 && value.lVal >= 0 && static_cast<unsigned>(value.lVal) == expected) ||
         (value.vt == VT_UI4 && value.ulVal == expected));
    VariantClear(&value);
    return matches;
}
inline HRESULT read_method(IWbemServices* services, BSTR path, const wchar_t* method,
                           std::optional<unsigned>& result) {
    Text name(method);
    if (!name.value) return E_OUTOFMEMORY;
    Pointer<IWbemCallResult> call;
    HRESULT status = services->ExecMethod(path, name.value, WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr, nullptr, nullptr, call.receive());
    if (FAILED(status)) return status;
    if (!call.value) return E_UNEXPECTED;
    Pointer<IWbemClassObject> output;
    status = call->GetResultObject(1500, output.receive());
    if (status == WBEM_S_TIMEDOUT) return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
    if (FAILED(status)) return status;
    if (!output.value) return E_UNEXPECTED;
    result = number(output.value, L"Data");
    return result ? S_OK : static_cast<HRESULT>(WBEM_E_TYPE_MISMATCH);
}
}
struct GamezoneFans {
    std::optional<unsigned> first, second;
    HRESULT error = S_OK;
    bool eligible = false;
};
inline GamezoneFans read_gamezone_fans(std::wstring_view model, std::wstring_view bios) {
    GamezoneFans fans;
    fans.eligible = manual_target(model, bios);
    if (!fans.eligible) return fans;
    using namespace gamezone_detail;
    Apartment apartment;
    fans.error = apartment.status;
    if (FAILED(fans.error)) return fans;
    Pointer<IWbemLocator> locator;
    Pointer<IWbemServices> services;
    fans.error = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator,
        reinterpret_cast<void**>(locator.receive()));
    if (FAILED(fans.error)) return fans;
    Text space(L"ROOT\\WMI"), className(L"LENOVO_GAMEZONE_DATA");
    if (!space.value || !className.value) { fans.error = E_OUTOFMEMORY; return fans; }
    fans.error = locator->ConnectServer(space.value, nullptr, nullptr, nullptr,
        WBEM_FLAG_CONNECT_USE_MAX_WAIT, nullptr, nullptr, services.receive());
    if (FAILED(fans.error)) return fans;
    fans.error = CoSetProxyBlanket(services.value, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
    if (FAILED(fans.error)) return fans;
    Pointer<IWbemClassObject> definition;
    fans.error = services->GetObject(className.value, 0, nullptr, definition.receive(), nullptr);
    if (FAILED(fans.error)) return fans;
    Pointer<IWbemQualifierSet> qualifiers;
    fans.error = definition->GetQualifierSet(qualifiers.receive());
    if (FAILED(fans.error)) return fans;
    VARIANT guid; VariantInit(&guid);
    fans.error = qualifiers->Get(L"Guid", 0, &guid, nullptr);
    const bool matches = SUCCEEDED(fans.error) && guid.vt == VT_BSTR && guid.bstrVal &&
        _wcsicmp(guid.bstrVal, L"{887B54E3-DDDC-4B2C-8B88-68A26A8835D0}") == 0;
    VariantClear(&guid);
    // Exact read-method allowlist; never enumerate-and-invoke provider methods.
    if (!matches || !method_matches(definition.value, L"GetFanCount", 7) ||
        !method_matches(definition.value, L"GetFan1Speed", 8) ||
        !method_matches(definition.value, L"GetFan2Speed", 9)) {
        fans.error = static_cast<HRESULT>(WBEM_E_INVALID_METHOD); return fans;
    }
    Pointer<IEnumWbemClassObject> rows;
    Text language(L"WQL"), query(L"SELECT * FROM LENOVO_GAMEZONE_DATA");
    if (!language.value || !query.value) { fans.error = E_OUTOFMEMORY; return fans; }
    fans.error = services->ExecQuery(language.value, query.value,
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, rows.receive());
    if (FAILED(fans.error)) return fans;
    IWbemClassObject* objects[2]{};
    ULONG count = 0;
    fans.error = rows->Next(1500, 2, objects, &count);
    Pointer<IWbemClassObject> instance, extra;
    instance.value = objects[0]; extra.value = objects[1];
    if (fans.error == WBEM_S_TIMEDOUT) { fans.error = HRESULT_FROM_WIN32(ERROR_TIMEOUT); return fans; }
    if (FAILED(fans.error)) return fans;
    if (count != 1 || !instance.value) { fans.error = static_cast<HRESULT>(WBEM_E_NOT_FOUND); return fans; }
    VARIANT path; VariantInit(&path);
    fans.error = instance->Get(L"__PATH", 0, &path, nullptr, nullptr);
    if (SUCCEEDED(fans.error) && path.vt == VT_BSTR && path.bstrVal) {
        std::optional<unsigned> fanCount, first, second;
        fans.error = read_method(services.value, path.bstrVal, L"GetFanCount", fanCount);
        if (SUCCEEDED(fans.error) && fanCount == 2u) {
            const HRESULT firstError = read_method(services.value, path.bstrVal, L"GetFan1Speed", first);
            const HRESULT secondError = read_method(services.value, path.bstrVal, L"GetFan2Speed", second);
            if (first) fans.first = fan_rpm(*first);
            if (second) fans.second = fan_rpm(*second);
            fans.error = FAILED(firstError) ? firstError : secondError;
            if (SUCCEEDED(fans.error) && (!fans.first || !fans.second)) fans.error = static_cast<HRESULT>(WBEM_E_TYPE_MISMATCH);
        } else if (SUCCEEDED(fans.error)) fans.error = static_cast<HRESULT>(WBEM_E_INVALID_PARAMETER);
    } else if (SUCCEEDED(fans.error)) fans.error = static_cast<HRESULT>(WBEM_E_INVALID_OBJECT_PATH);
    VariantClear(&path);
    return fans;
}
}
