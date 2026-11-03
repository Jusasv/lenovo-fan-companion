#include "diagnostic_rules.h"
#include <cstdlib>
#include <iostream>
#include <limits>

int main() {
    int failures = 0;
    int checks = 0;
    auto check = [&](bool condition, const char* name) {
        ++checks;
        if (!condition) { std::cerr << "FAIL: " << name << '\n'; ++failures; }
    };
    using namespace companion;
    check(vpc_hardware_ids(L"ACPI\\VEN_VPC&DEV_2004"), "unnamed Lenovo device vendor identifier");
    check(vpc_hardware_ids(L"ACPI\\VPC2004"), "legacy Lenovo identifier");
    check(vpc_hardware_ids(L"*VPC2004"), "wildcard Lenovo identifier");
    const wchar_t multiple[] = L"ACPI\\OTHER\0ACPI\\VPC2004\0\0";
    check(vpc_hardware_ids(std::wstring_view(multiple, sizeof(multiple) / sizeof(wchar_t))), "later multi-string hardware ID matched");
    check(!vpc_hardware_ids(L"ACPI\\VPC20040"), "different device not matched by prefix");
    check(!vpc_hardware_ids(L""), "missing hardware IDs not recognized");
    check(temperature(46.5) == 46.5, "valid temperature is preserved");
    check(!temperature(0), "zero does not mean a cool CPU");
    check(!temperature(-1), "negative sentinel is unavailable");
    check(!temperature(126), "implausible temperature rejected");
    check(!temperature(std::numeric_limits<double>::quiet_NaN()), "NaN rejected");
    check(!temperature(std::numeric_limits<double>::infinity()), "infinity rejected");
    check(sensor_kind(L"/intelcpu/0/temperature/0") == SensorKind::Cpu, "Intel CPU source");
    check(sensor_kind(L"/amdcpu/0/temperature/0") == SensorKind::Cpu, "AMD CPU source");
    check(sensor_kind(L"/gpu-nvidia/0/temperature/0") == SensorKind::Gpu, "NVIDIA GPU source");
    check(sensor_kind(L"/gpu-amd/0/temperature/0") == SensorKind::Gpu, "AMD GPU source");
    check(sensor_kind(L"/gpu-intel/0/temperature/0") == SensorKind::Gpu, "Intel GPU source");
    check(sensor_kind(L"/lpc/it8728f/temperature/0") == SensorKind::Other, "board sensor never becomes CPU");
    check(sensor_kind(L"/acpi/thermalzone/0") == SensorKind::Other, "ACPI zone never becomes CPU");
    check(sensor_kind(L"/intelcpu/0/fan/0") == SensorKind::Cpu, "kind classification does not replace query type filtering");
    std::optional<double> hottest;
    take_hottest(hottest, 0);
    check(!hottest, "invalid sensor does not create a reading");
    take_hottest(hottest, 52);
    take_hottest(hottest, 49);
    take_hottest(hottest, std::numeric_limits<double>::quiet_NaN());
    check(hottest == 52, "hottest valid reading retained");
    take_hottest(hottest, 71);
    check(hottest == 71, "hotter sensor selected");
    check(utf8(L"CPU 46\u00B0C") == "CPU 46\xC2\xB0" "C", "report degree sign UTF-8");
    check(utf8(L"\u8054\u60F3") == "\xE8\x81\x94\xE6\x83\xB3", "report international text UTF-8");
    if (failures) std::cout << "Diagnostic rules failed\n";
    else std::cout << checks << " diagnostic checks passed\n";
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
