# Validation records

The RX580 results below are for the **2048SP variant**, not the full 2304-shader RX580.

## Recorded hardware evidence

| | RX580 2048SP | RX 550 / Lexa |
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

RX580 2048SP example: **`0x00007242 → 66 °C`**, reference hotspot **66.00 °C**, edge **58.43 °C**.

RX 550 example: **`0x00005430 → 48 °C`**, reference hotspot **48.00 °C**, edge **43.43 °C**, graphics activity **100%**. Among 135 pairs with graphics activity ≥90%, mean absolute error vs hotspot was **0.0778 °C**. 149 of all 150 pairs were within 1 °C.

Both tests used Linux 4.4.0-21 without a bound graphics driver, a short graphics load, reference telemetry every 50 ms and register sampling approximately every 200 ms. The reference process group was briefly stopped during each indirect-register transaction and resumed after restoration; GPU work itself was not explicitly paused. Average/max process pauses were 5.062/6.050 ms on RX580 2048SP and 4.456/5.561 ms on RX 550. Actual maximum timestamp separation was 3.402 ms and 25.364 ms respectively (40 ms pairing limit).

RX 550 load completed successfully in **30.993 seconds**; total supervision was **32.871 seconds**. Hardware watchdog remained active (180 seconds, feed every 30 seconds). Original boot power mode was restored (214 MHz engine / 300 MHz memory); no reboot, new kernel messages or remaining test processes. No manual voltage, fan or clock values were set.

These results validate the firmware-reported hotspot on the two recorded boards, not external physical calibration or all Polaris models. Tool names and unrelated fields are omitted from the public evidence; original logs are retained in the bench archive. The reference collector is not distributed, so the public data supports independent recomputation of this comparison, not end-to-end reproduction of the reference acquisition.

## Check the evidence

[RX580 2048SP data](../evidence/) · [RX 550 data](../evidence/rx550/)

Each dataset includes register samples, a reduced reference CSV, matched pairs, comparison metrics, an analyzer and SHA-256 hashes. Reference timestamps are standardized to UTC; measured values and sample pairing are unchanged.

```sh
python3 evidence/analyze.py
python3 evidence/rx550/analyze.py
```

These commands regenerate the two analysis JSON files in each directory. Numeric results match the archived analysis within floating-point rounding; serialization and key order may differ.


## Windows RX580 2048SP � 2026-10-28

Short tests of the Windows reader completed with PCI access busy at startup, Afterburner, HWiNFO sensors, and both monitors under graphics load. The combined load run collected 20 valid readings, reaching GPU 57 �C and hotspot 66 �C, with a maximum delta of 10 �C. A subsequent HWiNFO-only run collected 12 valid readings. Normal and forced reader closure removed the owned service; a pre-existing monitor driver was preserved. The hardware watchdog remained active and the GPU reported no device error. These are compatibility checks on the tested configuration, not universal concurrency guarantees.

## RX550 validation

Tested on 2026-10-27 with RX550 `1002:699F`, revision C7, subsystem `1002:0B04`, Windows 10 Pro x64, display driver `31.0.12027.9001`. One visible 30-sample run at idle reported 25–26 °C; all 30 selector restorations passed. Median interval 1000 ms (984–1000 ms), sample span 28.984 seconds. Exit code 0, same boot, GPU error code 0, hardware watchdog running, no new display-recovery or WHEA events. The temporary driver service was removed on exit.

This validates the Windows collector at idle; the separate Linux evidence validates the register against reference telemetry under load. It is not a new Windows load test or validation of concurrent third-party monitoring.

[Raw samples, executable hash and test summary](../evidence/windows-rx550/).

**v0.1.1 discovery test:** RX550, 10 readings at 25 °C, median interval 1000 ms, 10/10 selector restorations, exit code 0. No adjacent driver file and no RTCore running before launch. The temporary service loaded the driver directly from the Afterburner installation and was removed on exit. Watchdog remained active; same boot and GPU error code 0.

**v0.1.2:** adds GPU temperature, signed delta and the Polaris icon. Both temperatures come from the same register. The icon build completed 10 readings with correct deltas and selector restoration on RX550, exit code 0. A preceding test of this collector under graphics load recorded 60 samples, reaching GPU 46 °C, hotspot 54 °C and a maximum delta of 10 °C; the supervisor did not save its final summary, so those readings are not presented as a full benchmark pass.
