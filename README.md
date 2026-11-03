# Lenovo Fan Companion

A small native Windows tray app for manual cooling requests on the Lenovo IdeaPad Y700.

**Public preview — tested on one Y700 configuration.** This is a manual utility, with no automatic cooling profiles or sustained Boost.

## Download and run

Download the **Windows x64 tray ZIP** from [preview 0.2](https://github.com/Jusasv/lenovo-fan-companion/releases/tag/v0.2.0-preview.1), extract it, and open `LenovoFanManual.exe`. Administrator approval is required to use the existing Lenovo driver. Opening the app sends no fan commands.

- **Normal cooling:** sends one Normal request. The initial tester observed the fans reduce speed after this request.
- **Boost test (3 sec):** sends one Boost/dust-removal request, waits up to three seconds after driver completion, then attempts Normal. **Stop test** ends that waiting period early.
- **Tray access:** Close hides the window. Click the fan icon to reopen it, or right-click for controls and **Exit**.

See [TRAY_GUIDE.md](TRAY_GUIDE.md) for everyday use and [MANUAL_TEST.md](MANUAL_TEST.md) for test scope and recovery limitations. The separate diagnostics ZIP contains `LenovoFanCompanion.exe`, which scans driver/sensor availability without changing fan settings; see [DIAGNOSTICS.md](DIAGNOSTICS.md).

The executable is unsigned. Windows security may block it on some systems; do not disable protections to run it.

## Tested configuration

| Item | Observed configuration |
| --- | --- |
| Laptop | Lenovo IdeaPad Y700-15ISK, product 80NV |
| BIOS | CDCN54WW |
| OS | Windows 11 25H2 |
| Lenovo ACPI driver | 15.10.28.170 |
| Manual behavior | Normal reduced fan speed; short manual test reported to behave as expected |
| Tray behavior | User confirmed hide and reopen work |

Controls are deliberately restricted to **80NV / CDCN54WW**. Other Y700 variants, BIOS versions and Lenovo models are unverified. This name leaves room for future models; it does not imply general Lenovo support.

The existing Lenovo ACPI virtual power driver must be installed. [Lenovo package DS119793](https://support.lenovo.com/om/en/downloads/DS119793) lists this laptop and Windows 10 support; it exposed the interface on the initial Windows 11 test machine. That is not general Windows 11 certification. See the [driver investigation](docs/y700-driver-investigation.md). No driver is bundled or installed by this app. PawnIO and LibreHardwareMonitor are **not required for the manual tray app**.

## Lightweight by design

The app uses native C++ and Windows libraries, with no Electron, .NET runtime, temperature polling, login task or periodic idle timer. Commands share one worker and cannot overlap within the app. A single-instance guard prevents a second copy in the same Windows session.

One 10.08-second idle sample of preview 0.2 on the initial laptop recorded about **17 MiB working set**, **2.2 MiB private memory**, and **0.00 CPU seconds**. These are short observations, not long-term benchmarks or guaranteed limits.

## Limits

This interface does not provide arbitrary fan percentages, independent fan control or measured RPM. Driver acceptance and observed fan response are reported separately.

The brief Boost test reuses a dust-removal command. It is **not sustained gaming cooling**. The reference utility's High mode historically cycled between fast and stopped fans on this laptop.

Exit attempts Normal after an active Boost test. A failed Normal request brings the window back for retry. A stuck synchronous driver call can delay restoration indefinitely; forced termination, sleep, crashes or power loss can prevent it. Keep your usual temperature monitor visible while testing. Automatic Quiet / Balanced / Gaming profiles, temperature-based recovery and unattended control are not implemented.

## Build and contribute

Use CMake and a Windows C++ compiler with the Windows SDK:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Portable rule/sequence tests also build on Linux. Windows/Linux CI covers diagnostic, manual-sequence, recovery and bounded-history checks, a read-only report smoke test, and device-disabled tray/recovery lifecycle checks. These tests do not establish physical fan behavior on other laptops.

See [CONTRIBUTING.md](CONTRIBUTING.md) and [design notes](docs/design.md). Compatibility reports should include model, BIOS, Windows version, driver version and observed behavior; omit serial numbers and full system dumps.

## Research and license

This is a separate project informed by [Lenovo-Fan-Control](https://github.com/jiarandiana0307/Lenovo-Fan-Control), [FanControl.LenovoPlugin](https://github.com/jiarandiana0307/FanControl.LenovoPlugin), and [Y700 controller research](https://github.com/hirschmann/nbfc/issues/526).

MIT; see [LICENSE](LICENSE) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
