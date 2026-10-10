#include "diagnostic_rules.h"
#include <cstdlib>
#include <iostream>
#include <limits>

int main() {
    int failures = 0;
    auto check = [&](bool condition, const char* name) {
        if (!condition) { std::cerr << "FAIL: " << name << '\n'; ++failures; }
    };
    using namespace companion;
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
    std::cout << (failures ? "Diagnostic rules failed\n" : "20 diagnostic checks passed\n");
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
