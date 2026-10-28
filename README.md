# Polaris Hotspot

Minimal C++ reader for GPU temperature and hotspot on AMD Polaris, validated under load on **RX580 2048SP** and **RX550 / Lexa**.

## Windows app

[Download PolarisHotspot.exe](https://github.com/fmuniztriana/polaris-hotspot/releases/latest/download/PolarisHotspot.exe) and run as administrator with MSI Afterburner installed. Requires Windows 10/11 x64 and exactly one Polaris GPU.

The app shows **GPU, Hotspot and Delta** every second. It finds the compatible RTCore driver automatically; Afterburner does not need to be open. Verified driver files from Afterburner **4.6.5, 4.6.6 and 4.6.7 Beta 2** are supported.

![RX580 2048SP under load, Advanced collapsed](windows/assets/panel.png)

**Advanced** shows cached GPU identification, active CUs/shaders, revision, ASIC Quality, memory vendor/model/capacity, VBIOS, SLT and raw SIDs. The memory model is marked **(VBIOS)** because it comes from the BIOS profile. Missing fields show N/A.

Busy PCI access is retried on the next second, including during startup. Other acquisition errors show N/A and the cause. The reader cleans up its own temporary service on exit and leaves a previously running driver alone.

[Windows instructions, driver details and build steps](windows/README.md)

## Supported and validated variants

Polaris 10/11/12 IDs are accepted, including RX470/480/570/580 variants. An **Untested profile** label does not block readings.

| Variant | Active units | Hardware validation |
|---|---|---|
| RX580 2048SP | 32 CUs / 2048 shaders | Validated under load |
| RX550 / Lexa | 8 CUs / 512 shaders | Validated under load |
| RX580 (full configuration) | 36 CUs / 2304 shaders | Accepted; not physically validated |

Validation applies to the recorded configurations, not every board sharing their PCI IDs. Testing the 2048SP variant does not validate the full RX580.

## Minimal register recipe

SMU register **`CG_MULT_THERMAL_STATUS = 0xC0300014`** contains hotspot in **bits 0–8** and GPU temperature in **bits 9–17**, in whole degrees Celsius. Access it through BAR5 byte offsets `0x210` (selector) and `0x214` (data):

```cpp
auto saved = read32(0x210);
write32(0x210, 0xC0300014);
auto raw = read32(0x214);
write32(0x210, saved);

auto hotspot_c = raw & 0x1FF;
auto gpu_c = (raw >> 9) & 0x1FF;
auto delta_c = int(hotspot_c) - int(gpu_c);
```

**Two reads and two writes**, excluding synchronization and validation. `read32` and `write32` are MMIO transport placeholders. DWORD-indexed transports use `0x84` and `0x85` instead.

The [C++ header](include/polaris_hotspot.hpp) adds selector checks and restoration on errors. The caller must coordinate access for the entire transaction. The Windows app uses `Global\Access_PCI`, which other drivers may not honor. This reads telemetry already maintained by the GPU.

## Evidence and example

[Validation records and test conditions](docs/validation.md) · [RX580 2048SP data](evidence/) · [RX550 data](evidence/rx550/)

Matched reference comparisons had mean absolute errors of **0.1169 °C** on RX580 2048SP and **0.0962 °C** on RX550. The datasets include analyzers and checksums. They validate the firmware reading against reference telemetry, not external physical calibration.

The standalone example decodes a raw word without accessing hardware:

```sh
c++ -std=c++11 -Wall -Wextra -Iinclude examples/decode.cpp -o decode
./decode 0x00005430
# 48 C
```

No kernel driver or proprietary firmware is bundled.
