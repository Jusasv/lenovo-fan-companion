# Lenovo Y700 driver investigation

The diagnostic now recognizes the Lenovo virtual power device even when Windows gives it no friendly name. It checks every entry in the hardware-ID list, including `ACPI\VEN_VPC&DEV_2004`, and reports Windows Code 28 separately from an absent device.

[Microsoft documents Code 28](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/cm-prob-failed-install) as a missing driver. An OS reinstall or upgrade can therefore leave the hardware present but its software interface unavailable; that explanation needs verification on each machine.

## Official package candidate

[Lenovo package DS119793](https://support.lenovo.com/bg/en/downloads/ds119793-advanced-configuration-and-power-management-interface-acpi-driver-for-windows-10-64-bit-notebook) lists the Y700-15ISK family and Windows 10 64-bit. Package version: 1.5.0.15; installer: `wwet030e.exe`.

The installer downloaded from Lenovo has a valid Lenovo Authenticode signature. Its SHA-256 matches Lenovo's published value:

```
DD7E89227554DFCDB05282F19EC396D3CC040090ECE0A35BA892452823B564AB
```

Windows 11 compatibility, the installed driver's availability, and whether it exposes `EnergyDrv` remain unverified. The package has not been installed during this investigation. No Lenovo installer or driver is bundled in this repository.

## Next hardware checks

After an explicitly approved driver installation, check Windows device status and whether the driver interface can be opened without sending commands. Installing the package alone does not prove fan control works. Fan command testing and temperature monitoring follow separately.

Historical reports of the reference utility's Low mode working before a reinstall do not establish support on the current installation. Its High mode uses a dust-removal operation; sustained cooling without full-speed/stopped cycling remains unverified.
