# Contributing

This project is at the investigation stage. Model-specific measurements, driver-interface research, and small reproducible findings are useful.

Before changing hardware behavior, describe the target laptop, firmware, Lenovo driver, intended operation, and recovery method. Keep transport code separate from policy and presentation.

Do not reuse embedded-controller register addresses from unrelated models. A reported RPM value must come from actual telemetry, not a requested mode or a fabricated percentage.

Compatibility reports should compare ordinary firmware Auto with the tested behavior and include the observation method. Sensor readings should identify their source. Report failures and cycling as well as successful cases.

Do not include credentials, serial numbers, account names, machine names, or complete firmware/system dumps. Small redacted excerpts are preferable.

Upstream code must retain the copyright and license notices its license requires. Link research sources and distinguish verified behavior from hypotheses.

Once an executable exists, contributions that change control behavior should include meaningful tests of mode transitions, failures, stale sensor data, and fallback behavior. Hardware compatibility requires physical testing in addition to software tests.
