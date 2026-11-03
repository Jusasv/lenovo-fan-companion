# Preview 0.2 — manual tray controls

An experimental Windows x64 preview, tested on Lenovo IdeaPad Y700-15ISK **80NV / BIOS CDCN54WW**, with Windows 11 25H2 and Lenovo ACPI driver 15.10.28.170. Controls stay disabled on other model/BIOS combinations.

## Downloads

- **LenovoFanCompanion-0.2.0-preview.1-windows-x64.zip** — manual tray app. Extract and open `LenovoFanManual.exe`.
- **LenovoFanCompanion-diagnostics-0.2.0-preview.1-windows-x64.zip** — separate read-only diagnostic app, `LenovoFanCompanion.exe`.
- **SHA256SUMS.txt** — SHA-256 checksums for both ZIPs. Checksums verify the downloaded files, not executable signing.

The manual app requires the existing Lenovo ACPI driver and administrator approval. It does not install drivers. LibreHardwareMonitor, PawnIO, .NET and Electron are not required for manual use.

## Included

Normal cooling, a short Boost test followed by a Normal request, Stop, a tray menu, hide/reopen and explicit Exit. No commands are sent on launch. No startup task, temperature polling or automatic cooling mode is added.

The initial user observed Normal reduce fan speed and reported the short test worked as expected. Tray hide/reopen was confirmed. A brief idle sample recorded approximately 17 MiB working set and 0.00 CPU seconds over 10.08 seconds.

Windows and Linux checks pass, including 38 portable rule/sequence checks and device-disabled startup/tray tests. Physical behavior has not been tested on other models.

## Limits

Boost is a brief dust-removal request, not sustained cooling or arbitrary speed control. There is no measured RPM. Driver acceptance does not prove fan response.

Exit attempts Normal after an active test; a failed request brings the window back for retry. Driver hangs, sleep, crashes, forced termination or power loss can prevent restoration. Keep your usual temperature monitor visible while testing.

This build is unsigned. Windows may block it; do not disable security protections to run it. Lenovo's relevant legacy driver package lists Windows 10 support; successful use on this one Windows 11 machine is not universal compatibility.

Source and usage: https://github.com/Jusasv/lenovo-fan-companion
