# Y700 fan-control capabilities: firmware and read-only investigation

Scope: the initial Y700-15ISK, product 80NV, BIOS CDCN54WW. Launch at login is postponed; the next requested direction is manual speed adjustment or a fan curve.

## What is now observed

A local copy of the DSDT was obtained with Windows GetSystemFirmwareTable and disassembled offline. Its ACPI header length/checksum validated. The raw AML and disassembly remain private working files; neither is included in this repository.

The firmware exposes the Lenovo GameZone WMI provider, GUID `{887B54E3-DDDC-4B2C-8B88-68A26A8835D0}`, object AA. Its inspected WMAA method implements:

| WMI method | ID | Observed firmware behavior |
| --- | --- | --- |
| GetFanCount | 7 | Returns 2 |
| GetFan1Speed | 8 | Returns FANS multiplied by 100 |
| GetFan2Speed | 9 | Returns FA2S multiplied by 100 |
| GetFanMaxSpeed | 10 | Returns constant 3400; not proof of a physical maximum |
| GetThermalTableID | 2 | Reads IRTI |
| SetThermalTableID | 3 | Writes a boolean IRTI value; actual cooling effect untested |
| SetFanCooling | 13 | Selects two EC command values, on/off; not an RPM/percentage target |
| GetCPUTemp / GetGPUTemp | 18 / 19 | Return constant zero in this implementation; not usable temperatures |

Three consecutive administrator-level read-only WMI probes reported fan 1 at **2600 RPM** and fan 2 at **2300 RPM**. The reported count was 2, thermal-table ID 0 and cooling status 0. These are vendor firmware reports, not independent tachometer measurements. No setters or fan-mode changes were invoked in this investigation.

The native diagnostics extension allows only GetFanCount and GetFan1Speed/GetFan2Speed, checks their GUID/method IDs, and preserves missing or malformed results as unavailable. A reported zero is distinguished from a failed read. It does not expose general-purpose WMI invocation or raw EC access. Read-result waits are finite; this does not establish a hard deadline for every COM/provider operation.

## What this does not establish

There is no arbitrary-speed setter in the inspected GameZone method. That does not prove the EC cannot support one: not all firmware/control paths are understood, and the standalone DSDT disassembly has unresolved external methods.

The existing EnergyDrv path is a mode/dust-removal workaround. The [reference FanControl plugin](https://github.com/jiarandiana0307/FanControl.LenovoPlugin) explicitly maps input percentages to a few modes, rather than providing accurate speed adjustment. A percentage slider over that path would be misleading.

The [earlier Y700 investigation](https://github.com/hirschmann/nbfc/issues/526) reports NVIDIA-state interactions, ineffective/overridden control and fan behavior becoming stuck with an experimental temperature-spoofing profile. Those are the author's observations, not proof of our laptop's cause. Such temperature spoofing is not adopted here.

No EC register writes, sensor spoofing, BIOS modification or new cooling setter calls were performed. A field named like a fan/thermal control is only a research lead; it is not a verified control register.

## Next controlled experiment

1. Use the verified read-only RPM interface to compare baseline, the existing short dust-removal test and post-Normal recovery. Observe CPU and GPU temperatures independently.
2. Implement the alternative GameZone cooling switch as a separate, explicitly initiated, brief experiment with its matching on/off restoration and the same serialized recovery guard. Validate actual RPM response and recovery before calling it a usable Boost mode.
3. Investigate a real speed-target interface only if firmware evidence identifies one. Verify stable nonzero intermediate speeds and repeatability across GPU power states before offering a manual percentage/RPM control.
4. Add a temperature policy only after the supported output levels are established. It needs fresh CPU/GPU readings, hysteresis, dwell time, handling of missing sensors and a verified fallback. A two-state policy must be presented as mode switching, not a continuous fan curve.

Current result: RPM observation is feasible on this configuration; arbitrary manual speed and a true custom curve remain unverified.

## Sources

- [Microsoft GetSystemFirmwareTable](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getsystemfirmwaretable)
- [ACPICA official releases / offline disassembler](https://github.com/open-acpica/acpica/releases)
- [Original EnergyDrv implementation](https://github.com/jiarandiana0307/Lenovo-Fan-Control/blob/main/src/fanctrl.c)
- [FanControl.LenovoPlugin mode mapping](https://github.com/jiarandiana0307/FanControl.LenovoPlugin)
- [Y700-15ISK NBFC investigation](https://github.com/hirschmann/nbfc/issues/526)
