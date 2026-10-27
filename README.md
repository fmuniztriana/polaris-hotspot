# Polaris hotspot

Minimal C++ recipe for reading the firmware-reported hotspot on AMD Polaris, validated under load on an RX 580 and an RX 550 (Lexa).

## Windows display

[Download PolarisHotspot.exe](https://github.com/fmuniztriana/polaris-hotspot/releases/latest) and run as administrator with MSI Afterburner installed. It shows **GPU, Hotspot and Delta**, updated every second, and finds the compatible RTCore driver automatically. No copying files or opening Afterburner is needed.

![Polaris Hotspot panel](windows/assets/panel.png)

Both temperatures come from the same register read; delta is `Hotspot − GPU`. See [setup and build instructions](windows/README.md).

Accepts Polaris 10/11/12, including RX 470/480/570/580 variants, even when the board profile has not been tested. The panel marks **Perfil não testado** without blocking readings. If acquisition fails, it shows **N/A / Leitura indisponível**. One Polaris GPU at a time; hardware validation remains limited to the recorded RX550/RX580 boards. The screenshot above illustrates the temperature layout; v0.1.3 also adds the profile status below it.

## The entire recipe

SMU register **`CG_MULT_THERMAL_STATUS = 0xC0300014`**, field **`ASIC_MAX_TEMP`, bits 0–8**, contains whole degrees Celsius. Access it through BAR5 byte offsets `0x210` (selector) and `0x214` (data):

```cpp
auto saved = read32(0x210);
write32(0x210, 0xC0300014);
auto raw = read32(0x214);
write32(0x210, saved);
auto hotspot_c = raw & 0x1FF;
```

**Two reads and two writes**, excluding synchronization and validation. The functions above are MMIO transport placeholders, not OS APIs. DWORD-indexed transports use indices `0x84` and `0x85` instead of byte offsets.

The [C++ header](include/polaris_hotspot.hpp) adds selector checks and restoration on errors. The caller must coordinate access to the shared selector for the entire transaction. A cooperating Windows monitor can use `Global\Access_PCI`; this does not guarantee cooperation from every driver. No DRAM command, rendering hook or sensor mailbox request is needed. This reads telemetry already maintained by the GPU.

## Validated boards

| | RX 580 | RX 550 / Lexa |
|---|---|---|
| PCI device | `1002:6FDF` | `1002:699F` |
| Revision / subsystem | `EF` / `1682:E580` | `C7` / `1002:0B04` |
| Date | 2026-10-20 | 2026-10-27 |
| Register samples / restored selectors | 67 / 67 | 158 / 158 |
| Matched reference pairs | 59 | 150 |
| Mean absolute error vs reference hotspot | 0.1169 °C | 0.0962 °C |
| Maximum absolute error vs reference hotspot | 1.37 °C | 1.41 °C |
| Mean absolute error vs reference edge | 5.2571 °C | 3.9293 °C |
| Observed hotspot range | 34–66 °C | 28–48 °C |

RX 580 example: **`0x00007242 → 66 °C`**, reference hotspot **66.00 °C**, edge **58.43 °C**.

RX 550 example: **`0x00005430 → 48 °C`**, reference hotspot **48.00 °C**, edge **43.43 °C**, graphics activity **100%**. Among 135 pairs with graphics activity ≥90%, mean absolute error vs hotspot was **0.0778 °C**. 149 of all 150 pairs were within 1 °C.

Both tests used Linux 4.4.0-21 without a bound graphics driver, a short graphics load, reference telemetry every 50 ms and register sampling approximately every 200 ms. The reference process group was briefly stopped during each indirect-register transaction and resumed after restoration; GPU work itself was not explicitly paused. Average/max process pauses were 5.062/6.050 ms on RX 580 and 4.456/5.561 ms on RX 550. Actual maximum timestamp separation was 3.402 ms and 25.364 ms respectively (40 ms pairing limit).

RX 550 load completed successfully in **30.993 seconds**; total supervision was **32.871 seconds**. Hardware watchdog remained active (180 seconds, feed every 30 seconds). Original boot power mode was restored (214 MHz engine / 300 MHz memory); no reboot, new kernel messages or remaining test processes. No manual voltage, fan or clock values were set.

These results validate the firmware-reported hotspot on the two recorded boards, not external physical calibration or all Polaris models. Tool names and unrelated fields are omitted from the public evidence; original logs are retained in the bench archive. The reference collector is not distributed, so the public data supports independent recomputation of this comparison, not end-to-end reproduction of the reference acquisition.

## Check the evidence

[RX 580 data](evidence/) · [RX 550 data](evidence/rx550/)

Each dataset includes register samples, a reduced reference CSV, matched pairs, comparison metrics, an analyzer and SHA-256 hashes. Reference timestamps are standardized to UTC; measured values and sample pairing are unchanged.

```sh
python3 evidence/analyze.py
python3 evidence/rx550/analyze.py
```

These commands regenerate the two analysis JSON files in each directory. Numeric results match the archived analysis within floating-point rounding; serialization and key order may differ.

## Minimal C++ example

```sh
c++ -std=c++11 -Wall -Wextra -Iinclude examples/decode.cpp -o decode
./decode 0x00005430
# 48 C
```

MSVC developer command prompt:

```bat
cl /nologo /EHsc /W4 /Iinclude examples\decode.cpp /Fe:decode.exe
decode.exe 0x00005430
```

This example decodes a raw word without accessing hardware. For live reads, supply an MMIO transport to the header. No kernel driver, proprietary binary, firmware image or memory-temperature collector is included.
