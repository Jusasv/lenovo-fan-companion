#pragma once
#include <cmath>
#include <cwctype>
#include <optional>
#include <string>
#include <string_view>

namespace companion {
inline bool vpc_hardware_ids(std::wstring_view identifiers) {
    // Windows supplies a REG_MULTI_SZ list; a device without a driver may have no name.
    for (std::size_t start = 0; start < identifiers.size();) {
        const auto end = identifiers.find(L'\0', start);
        auto id = std::wstring(identifiers.substr(start, end == std::wstring_view::npos ? end : end - start));
        for (auto& character : id) character = static_cast<wchar_t>(std::towlower(character));
        if (id == L"acpi\\ven_vpc&dev_2004" || id == L"acpi\\vpc2004" || id == L"*vpc2004") return true;
        if (end == std::wstring_view::npos) break;
        start = end + 1;
    }
    return false;
}

inline std::optional<double> temperature(double value) {
    // Reject missing, invalid and implausible readings, never convert them to zero.
    if (!std::isfinite(value) || value <= 0.0 || value > 125.0) return std::nullopt;
    return value;
}

enum class SensorKind { Other, Cpu, Gpu };
inline bool absolute_temperature_name(std::wstring_view name) {
    if (name.empty()) return false;
    std::wstring lower(name);
    for (auto& character : lower) character = static_cast<wchar_t>(std::towlower(character));
    return lower.find(L"distance") == std::wstring::npos && lower.find(L"tjmax") == std::wstring::npos;
}

inline SensorKind sensor_kind(std::wstring_view identifier) {
    // Only identifiers from explicitly queried monitor temperature sensors qualify.
    if (identifier.rfind(L"/intelcpu/", 0) == 0 || identifier.rfind(L"/amdcpu/", 0) == 0)
        return SensorKind::Cpu;
    if (identifier.rfind(L"/gpu-nvidia/", 0) == 0 || identifier.rfind(L"/gpu-amd/", 0) == 0 ||
        identifier.rfind(L"/gpu-intel/", 0) == 0) return SensorKind::Gpu;
    return SensorKind::Other;
}

inline void take_hottest(std::optional<double>& current, double candidate) {
    const auto valid = temperature(candidate);
    if (valid && (!current || *valid > *current)) current = valid;
}

inline std::string utf8(std::wstring_view input) {
    std::string output;
    for (std::size_t i = 0; i < input.size(); ++i) {
        auto code = static_cast<unsigned int>(input[i]);
        if constexpr (sizeof(wchar_t) == 2) {
            if (code >= 0xD800 && code <= 0xDBFF) {
                if (i + 1 < input.size()) {
                    const auto low = static_cast<unsigned int>(input[i + 1]);
                    if (low >= 0xDC00 && low <= 0xDFFF) {
                        code = 0x10000 + ((code - 0xD800) << 10) + low - 0xDC00;
                        ++i;
                    } else code = 0xFFFD;
                } else code = 0xFFFD;
            } else if (code >= 0xDC00 && code <= 0xDFFF) code = 0xFFFD;
        }
        if (code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) code = 0xFFFD;
        if (code < 0x80) output.push_back(static_cast<char>(code));
        else if (code < 0x800) {
            output.push_back(static_cast<char>(0xC0 | (code >> 6)));
            output.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        } else if (code < 0x10000) {
            output.push_back(static_cast<char>(0xE0 | (code >> 12)));
            output.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        } else {
            output.push_back(static_cast<char>(0xF0 | (code >> 18)));
            output.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        }
    }
    return output;
}
}
