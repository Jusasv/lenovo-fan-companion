# Diagnostics preview 0.1

This build inspects local driver and sensor availability. It **does not control fans**, install a driver, apply cooling profiles, start automatically, or send data over the network.

## Run

Download the Windows x64 diagnostic artifact from a successful **Build diagnostics** run in GitHub Actions. Extract the archive and open `LenovoFanCompanion.exe`. This is an unsigned development executable, not a verified cooling controller. Do not disable Windows security protections to run it.

The window displays laptop identity, Lenovo virtual power driver status, CPU/GPU temperature availability, and a detailed report. Use **Refresh** for another snapshot. **Copy report** and **Save report** act only when selected. Review the report before sharing it.

For a report without opening the main window:

```powershell
./LenovoFanCompanion.exe --report ./diagnostics.txt
```

The report includes generic model and firmware information, Windows version, relevant driver metadata, sensor availability, and scan time. It omits serial numbers, usernames, computer names, full device identifiers, and raw firmware data. It is never uploaded automatically.

## What the results mean

- **Device found** means Windows lists a present virtual power device. It does not prove fan-control support.
- **Read handle opened** means the Lenovo `EnergyDrv` endpoint can be opened for reading. No IOCTLs or mode commands are sent.
- **Access denied / busy** leaves endpoint availability unresolved; it is not evidence of unsupported hardware.
- **CPU temperature** comes only from an existing LibreHardwareMonitor WMI temperature sensor with a recognized CPU identifier. The hottest valid CPU sensor is shown. No ACPI thermal zone is labeled as CPU temperature.
- **GPU temperature** prefers the installed NVIDIA NVML interface. An existing recognized LibreHardwareMonitor GPU sensor is a fallback. NVML may be unavailable on older GPUs or when the discrete GPU is asleep. Querying it may wake the GPU temporarily.
- **Unavailable** is not zero. Missing, invalid, and implausible readings remain unavailable.
- These are snapshots. Freshness of an external monitor's readings is unverified, so these readings are not yet suitable for unattended quiet-mode control.

LibreHardwareMonitor does not need to be installed for the app to run. The app only checks whether its WMI provider already exists. NVIDIA libraries are loaded only from fixed Windows/vendor locations; no DLL is downloaded or loaded from the working directory.

## Build

Install a C++ compiler with the Windows SDK and CMake, then:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The native app uses Windows libraries and a static C++ runtime; it does not need .NET, Electron, or a separate Visual C++ runtime installation. Portable diagnostic-rule tests also build on Linux.

## Limits

Quiet, Balanced, Gaming, automatic curves, tray controls, RPM telemetry, and boost are not implemented in this diagnostic preview. It does not determine a safe temperature threshold or certify compatibility. The initial Y700 report of High-mode cycling remains an open investigation.

## Implementation references

- [Microsoft SetupAPI device enumeration](https://learn.microsoft.com/en-us/windows/win32/api/setupapi/nf-setupapi-setupdigetclassdevsw)
- [Microsoft WMI C++ guidance](https://learn.microsoft.com/en-us/windows/win32/wmisdk/creating-a-wmi-application-using-c-)
- [NVIDIA NVML temperature interface](https://docs.nvidia.com/deploy/nvml-api/latest/api/group__nvmlDeviceQueries.html)
- [LibreHardwareMonitor WMI example](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/blob/master/LibreHardwareMonitor.Windows.Forms/TestScripts/basicwmi.py)
