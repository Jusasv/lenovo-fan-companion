# Manual Y700 test preview

This is a separate experimental executable, LenovoFanManual.exe. It sends commands only when you press a control. It has no startup task, automatic curves, temperature polling or sustained Boost loop.

Controls are limited to model 80NV with BIOS CDCN54WW. Initial manual behavior has been observed on this target; the scope and limits are recorded below. Windows administrator approval is required. The Lenovo ACPI virtual power driver must already be installed.

## Observed behavior on the initial target

On model 80NV / BIOS CDCN54WW with Windows 11 and Lenovo ACPI driver 15.10.28.170, the user confirmed that Normal was accepted and the fans reduced speed. The user subsequently reported that the manual tests worked as expected after being asked to try the short Boost test.

These are user observations, not RPM measurements or independently timed tests. Sustained Boost, automatic curves, repeated-use reliability and recovery from forced termination or sleep remain unverified. Other models and BIOS versions have not been tested.

Use your existing monitor for both CPU and GPU temperatures. First try **Normal cooling** when temperatures have settled but fans remain loud. It sends one Normal command, checks the Windows driver result, and leaves observation to you. It does not deliberately hold the fans at low speed.

**Boost test (3 sec)** sends one Boost/dust-removal command, waits at most three seconds after driver completion, then attempts Normal. **Stop test** ends that waiting period early. Observe both fans; the reference High behavior has cycled between fast and stopped fans on the target laptop. This is not a sustained cooling mode.

The UI reports driver acceptance or an actual Windows error code. It never presents acceptance as measured fan speed. There is no RPM telemetry, and Normal does not prove firmware control has been restored.

Closing during a test requests Stop and waits for a Normal command before exiting. If Normal fails, the window remains open for retry. Synchronous driver calls can delay commands and restoration indefinitely; the three-second limit applies to the waiting period, not a stuck driver. Sleep, forced termination, crashes and power loss can prevent restoration. Test only while present, outside gaming, with temperatures visible.

No driver query that changes mode for calibration is used. No automatic fan commands occur at startup. The existing diagnostics executable and its report path remain separate and read-only.

The build is unsigned. If Windows Application Control blocks it, do not disable protections. Signing/distribution remains unresolved.

Protocol research: [Lenovo-Fan-Control](https://github.com/jiarandiana0307/Lenovo-Fan-Control/blob/main/src/fanctrl.c). See THIRD_PARTY_NOTICES.md. No Lenovo driver is bundled.
