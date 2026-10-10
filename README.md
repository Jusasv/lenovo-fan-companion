# Lenovo Fan Companion

A lightweight Windows fan-control project for supported Lenovo laptops.

**Status: diagnostic preview in development. Fan control is not implemented; hardware support is under investigation.**

The first native Windows app inspects the Lenovo virtual power driver and available CPU/GPU temperature sources without sending fan commands. See [DIAGNOSTICS.md](DIAGNOSTICS.md) for download/build instructions and limitations. Windows and Linux checks pass, including 20 diagnostic-rule checks and a Windows report smoke test. The report also ran locally; driver and sensor availability still need to be established before fan control can be implemented.

The initial test target is the Lenovo IdeaPad Y700. The name leaves room for other models; it does not imply universal Lenovo compatibility.

## Intended behavior

| Situation | Intended behavior |
| --- | --- |
| Idle | Quiet cooling, only where the hardware method and temperature safeguards are verified |
| Everyday work / medium load | Lenovo firmware-managed cooling |
| Gaming / sustained high load | Stronger cooling, once a stable boost method is verified |

The planned app has a small native Windows window, a tray icon, optional startup, and clear mode/status reporting. Adaptive behavior needs reliable CPU and GPU temperature readings, hysteresis, and a defined response to missing sensors.

## Why support must be verified

The reference project uses Lenovo’s ACPI-Compliant Virtual Power Controller driver and its dust-removal operation. That mechanism does not establish arbitrary RPM or percentage control.

Our initial Y700 testing feedback says the reference utility’s Low mode works well, but High mode cycles between fast fans and stopped fans. Sustained Boost is therefore **unverified**, rather than an advertised feature.

The reference source’s Low mode repeatedly sends the Normal command. We need to measure its behavior against ordinary firmware control before calling it a dependable quiet mode.

## Development plan

1. Identify the installed Lenovo driver and expose accurate diagnostics without changing fan settings.
2. Establish a baseline for firmware Auto and the reference Low behavior.
3. Investigate High-mode cycling and verify a stable boost method, if the firmware allows one.
4. Verify temperature sources and recovery after errors, exit, sleep/resume, and process termination.
5. Add adaptive Idle / Everyday / Gaming behavior and publish an accurately labeled test build.

A driver opening successfully does not prove fan control works. We will report command success separately from observed fan behavior, and show RPM only when actual telemetry is available.

## Compatibility

| Family | Status |
| --- | --- |
| IdeaPad Y700 | Initial development target; no released controller |
| Other Lenovo laptops | Unverified; model-specific investigation required |

Please use the compatibility-report issue template for reports. Do not upload serial numbers or full system/firmware dumps.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) and [the design notes](docs/design.md). The native diagnostic app, portable interpretation tests, and Windows/Linux build workflow form the first implementation step.

## Research and attribution

This is a separate project, informed by:

- [Lenovo-Fan-Control](https://github.com/jiarandiana0307/Lenovo-Fan-Control) — driver protocol and dust-removal workaround.
- [FanControl.LenovoPlugin](https://github.com/jiarandiana0307/FanControl.LenovoPlugin) — the same interface integrated with FanControl.
- [Lenovo Y700 dust-removal documentation](https://iknow.lenovo.com.cn/detail/157265).
- [Y700-15ISK embedded-controller investigation](https://github.com/hirschmann/nbfc/issues/526).

Any incorporated upstream code must retain its required copyright and license notices.

## License

MIT. See [LICENSE](LICENSE).
