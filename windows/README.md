# Minimal Windows hotspot display

`PolarisHotspot.exe` displays GPU temperature, hotspot and their delta once per second. Close the window to stop. It does not start a graphics workload or query memory temperatures.

All three values come from one RTCore register read: GPU is `CTF_TEMP` (bits 9–17), hotspot is `ASIC_MAX_TEMP` (bits 0–8), and delta is hotspot minus GPU. Both temperatures use whole degrees Celsius; GPU is the SMU7 temperature field used by the Linux driver, not a fractional or averaged reading from a monitoring API. Temperature collection does not use ADL; Advanced optionally uses the installed driver's ADL library for VBIOS and memory identification.

**Quick start:** with MSI Afterburner installed, download the executable and run it as administrator. The compatible driver is found automatically; no file copying is needed.

Windows 10/11 x64, administrator rights, and exactly one Polaris GPU are required. Polaris 10/11/12 IDs are accepted, including `67DF` used by RX470/480/570/580 variants, plus the tested `6FDF` and `699F` profiles. Other Polaris boards are allowed even without bench validation. Detection uses the Windows PCI location; no bench-specific address is compiled in.

The panel distinguishes **Tested profile** (a previously tested PCI/subsystem/revision profile) from **Untested profile**. This label does not block collection or certify every board sharing those IDs. A failed or invalid reading shows **N/A** and the cause, with no stale temperatures. Unsupported families are not probed. The accepted IDs come from the [Linux Polaris PCI table](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/amd/amdgpu/amdgpu_drv.c) and are listed in `include/polaris_devices.hpp`.

**Validated variants:** RX580 2048SP (32 CUs / 2048 shaders) and RX550/Lexa (8 CUs / 512 shaders). The full RX580 (36 CUs / 2304 shaders) is accepted but has not been physically validated in the recorded tests.

## Advanced identification

**Advanced** is collapsed on startup. It shows the Windows GPU name, active CUs/shaders, PCI revision, ASIC Quality, VBIOS and raw primary/extended SID. Identification is read once when PCI access first becomes available; temporary contention at startup is retried on the next one-second tick. Opening or closing this section only displays cached data. Fields that cannot be read remain N/A.

Memory vendor and capacity are queried from the installed driver's identification API once at startup. Memory model is read from the active ATOM VBIOS memory module and explicitly marked **(VBIOS)**; this is the BIOS declaration, not a physical per-chip ID. The ROM read uses the coordinated SMU window, preserves both selectors and does not issue DRAM mode commands or program the ROM. Unsupported table formats or inconsistent reads leave the model as N/A. The panel caches all three fields.

ASIC Quality uses the already-read factory word 14: `((word >> 4) & 0xFFF) * 0.25 / 1023 * 100`, displayed to one decimal place. No additional hardware reads are needed. This is the factory quality indicator, not a health test or a guarantee of overclocking performance.

CU counts come from the hardware and user inactive-unit masks; shaders are active CUs × 64. SID and unit masks are read through RTCore, with access selectors restored. No fuse or unit-disable mask is programmed. Name comes from Windows device enumeration; VBIOS uses the installed driver's identification API once, because the registry can contain a placeholder. Thermal readings remain entirely through RTCore. The Windows name may describe a product family rather than an exact retail model, and SID is not a board serial number.

SLT is converted once from the cached SIDs: primary bits 0–31 encode the lot in base 37 (space, A–Z, 0–9); primary bits 32–36 encode the wafer, bits 37–44 encode X, and extended bits 0–7 encode Y. The decimal numeric fields are appended with a minimum width of two digits. Both raw SIDs remain visible because SLT does not preserve all their bits. No extra hardware reads are made for this conversion.

## Register transport

This application needs a compatible RTCore64 register-access driver. A kernel driver is not embedded or redistributed here. A driver already exposing `\\.\RTCore64` can be used directly; an incompatible interface causes a transport error, not a temperature reading.

If no driver is already available, the program searches for a compatible `RTCore64.sys` beside the executable, then in the MSI Afterburner installation recorded in the Windows uninstall registry (32-bit and 64-bit views), and finally in the standard Program Files locations. An existing Afterburner installation can therefore be used directly, without copying its driver or opening the Afterburner application. Custom installations are discovered through their registered installation/uninstall location.

Each candidate is checked before loading. An incompatible candidate is skipped; if no supported file is found, the program displays an error. The following SHA-256 hashes cover the inspected Afterburner 4.6.5, 4.6.6 and 4.6.7 Beta 2 packages (the latter two contain the same driver):

```text
F1C8CA232789C2F11A511C8CD95A9F3830DD719CAD5AA22CB7C3539AB8CB4DC3
A4F44E267698D47F6A905E96B356582B4EE9CF4049E8C792FFDA1B7356E68D35
```

The program checks the hash before starting its temporary `PolarisHotspotRTCore` service. A hidden copy of the same executable watches the reader and requests the stop and removal of the service it created, including when the reader process is forcibly terminated. If another client has the driver open, Windows keeps it available until that client closes it, then completes the removal. A driver already running is reused and left alone. Only one reader instance is allowed while its cleanup is pending. If the reader and helper are both forcibly terminated, the helper cannot clean up; the service may remain loaded. Windows must permit loading the driver; the application does not change security settings. If cleanup fails, details are written to `%TEMP%\PolarisHotspot-cleanup.log`.

The temperature loop coordinates indirect SMU access with `Global\Access_PCI`, validates selection/restoration, and writes only the SMU selector. Startup identification also temporarily changes the CU selector and ROM read index, restoring their saved values. Other monitors and drivers may not honor this mutex; simultaneous monitoring is not universally validated.

If PCI access is busy for 200 ms, the panel shows **N/A / PCI access busy; retrying** and tries again on the next one-second tick. It never displays stale temperatures. Transport, invalid-value and selector/restoration failures stop collection and show the cause in the panel. Error text can be selected and copied, or scrolled with the keyboard if it exceeds the visible area; close and reopen after resolving the cause. Optional logs record skipped attempts as `BUSY` and fatal failures as `ERROR`. The `--samples` limit counts successful readings, not busy attempts.

## Build

Open an **x64 Visual Studio Developer Command Prompt**:

```bat
windows\build.cmd
```

Output: `build\PolarisHotspot.exe`. The C++ runtime is linked statically. The executable requests elevation on launch.

To stop after a specified number of successful readings and save a log:

```bat
PolarisHotspot.exe --samples 30 --log new-samples.csv
```

The log file must not already exist. Busy attempts do not count toward the sample limit, so this is not a wall-clock timeout. Each successful row records monotonic uptime, raw register value, hotspot and confirmed selector restoration. These options do not add anything to the normal window.

## Validation

Validated on RX550/Lexa and RX580 2048SP. The latter includes short tests with startup contention, Afterburner, HWiNFO sensors and both monitors under graphics load, plus normal and forced reader closure. These tests do not guarantee compatibility with every monitoring configuration.

[Detailed validation records](../docs/validation.md) � [RX550 Windows evidence](../evidence/windows-rx550/)
