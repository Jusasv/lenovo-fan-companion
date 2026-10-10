# Design and validation plan

## Product goal

Quiet at idle, normal cooling during everyday work, and stronger cooling during gaming. A native Windows application should keep background resource use modest. Exact resource targets will be measured after there is a working build.

## Architecture

Separate the user interface, temperature collection, adaptive policy, and hardware transport.

The policy chooses a requested mode. The backend exposes only capabilities verified for the detected hardware. A requested mode, successful driver command, and measured fan speed are different facts and must not be conflated.

All driver operations run through one serialized controller. Avoid concurrent workers changing modes. Check each operation's result, validate output lengths, and use bounded retries. Never create an endless retry loop when a driver fails.

## Modes and defaults

Start in firmware Auto. Do not apply fan changes merely to inspect compatibility.

Quiet is not a universal low-RPM setting. The reference's repeated Normal command requires testing, including whether it reduces cooling under load.

Boost is unavailable for daily use until stable operation is established on the target machine. The initial report of full-speed / stopped cycling must be reproduced and explained before release.

Adaptive mode requires fresh, reliable CPU and GPU readings. Use separate enter/exit thresholds and minimum dwell times. Thresholds depend on measured hardware behavior; no generic temperature values are approved yet.

If temperature readings are invalid or stale, leave Quiet and return to a verified fallback. Suspend adaptive control across sleep; revalidate the driver and sensors on resume. Ordinary exit must restore firmware Auto. Recovery after a forced kill or crash is a hardware-test requirement, not a promise.

## Read-only diagnosis first

Collect only the model, BIOS version, Windows version, and relevant driver identification needed for local diagnosis. Inspect locally first. A report copied or exported for sharing must omit personal identifiers and remain user-reviewable.

Opening a driver handle is not evidence that any requested fan mode works. Even a successful command does not prove a speed change; hardware behavior must be checked separately.

## Test gates

- Missing driver: clear explanation; no fan writes.
- Malformed driver response: reject; no unbounded retries or false success.
- Mode switching: a single controller; no stale worker reapplying an old mode.
- Missing or stale temperature: Quiet cannot remain forced.
- Thermal rise while Quiet: return to a validated cooling mode.
- Normal exit, sleep/resume, crash and forced termination: record the observed firmware recovery behavior.
- Boost: validate fan behavior and CPU/GPU temperatures under sustained load.
- Other models: require an explicit compatibility record rather than inheriting Y700 support.

No hardware writes will be enabled merely because the application compiles or software tests pass.

## Open questions

1. Which installed Lenovo driver and version exposes the interface?
2. What does repeatedly issuing Normal actually change on the Y700?
3. Is High-mode cycling intrinsic to dust removal, a timing issue, or another firmware behavior?
4. Can steady boost be achieved without bypassing the firmware's thermal behavior?
5. Which reliable CPU/GPU sensor sources work on the target Windows installation?
6. Does a future hardware backend need a separate privileged component?
