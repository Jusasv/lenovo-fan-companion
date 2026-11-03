# Lenovo Fan Companion — tray preview 0.2

Extract the archive, then open `LenovoFanManual.exe`. Windows requests administrator approval because the existing Lenovo driver requires it. Opening the app sends no fan commands. Close any earlier manual preview before opening this version.

- **Normal cooling** sends one Normal request. This reduced fan speed during user testing on the initial Y700.
- **Boost test (3 sec)** sends one Boost request and attempts Normal after a three-second waiting period. **Stop test** ends that wait early. This is still a short test, not sustained cooling.
- **Close** and **Hide to tray** keep the app running in the notification area. Click its fan icon to reopen the window; right-click for controls and **Exit**. Windows may initially put the icon under the taskbar's hidden-icons arrow.
- **Exit** stops an active test and waits for a Normal request before quitting. If Normal fails, the window opens and stays available for retry. A stuck driver, forced termination, sleep or power loss can prevent restoration; see MANUAL_TEST.md.

The app uses no temperature polling or periodic background timer during normal operation. It adds no startup task and does not install a driver or change login settings. It permits one copy per Windows session. If the notification area is unavailable, the app stays visible and Close exits instead. If Explorer restarts, the icon is registered again; failure brings back the window.

Keep your usual temperature monitor open when testing. Buttons and tray menu commands share the same serialized worker, and concurrent mode commands are disabled. Tooltips show the latest command status, not actual fan speed. There is no RPM telemetry or automatic Quiet/Gaming mode.

Controls remain restricted to model 80NV / BIOS CDCN54WW with an existing Lenovo ACPI driver. The app is unsigned; Windows security may block it on some systems. Do not disable protections to run it.

The CI `--ui-smoke-test` option exercises window/tray lifecycle with device access disabled, then exits. It is for testing only. Exit code 3 means the desktop has no accessible tray and only the window fallback was tested; code 2 means another instance prevented the UI test. The normal `--self-check` exits before any window or device access.
