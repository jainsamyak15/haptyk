# Sensor notes (Milestone 0)

Findings about the MacBook motion sensor. Every value here is measured, never assumed. Fill each
section, then ask the owner for the go/no-go decision.

Spec: docs/SPEC.md, "Milestone 0: sensor discovery (go/no-go gate)".

**Status (2026-10-24): the stream, layout, scale, rate and a first keyed protocol are measured on
Mac17,3.** A normal-user write of the accelerometer driver's `ReportInterval` to 2500 microseconds
starts reporting; its original zero value is restored when Keyfeel stops (ADR-0011). The idle
capture measured the 22-byte layout, `1 / 65,536 g` scale, a 399.0 Hz effective rate, and the
report timestamp clock. Desk and lap keyed captures measured timing and row separation, but did
not meet a universal force-tier quality gate.

## Test machines

| Mac model (`hw.model`) | Model name | Chip | macOS | Date | Notes |
| --- | --- | --- | --- | --- | --- |
| Mac17,3 | MacBook Air | Apple M5 | 27.0 (26A428) | 2026-10-21 | Discovery and permission probes |
| Mac17,3 | MacBook Air | Apple M5 | 27.0 (26A428) | 2026-10-24 | Root probe, driver wake and idle capture |

## Tools

| Tool | Use |
| --- | --- |
| `build/debug/bin/hid-dump [--json] [matching flags]` | Every HID device and its properties. Read-only. |
| `build/debug/bin/hid-dump --probe <matching flags>` | Times the presence check the app would run |
| `build/debug/bin/sensor-log <matching flags> ...` | CSV of every report plus KEYDOWN/KEYUP markers (no key codes) |
| `Tools/py/m0_protocol.py --surface desk\|lap` | Guided test protocol; writes labelled logs to `logs/` |
| `Tools/py/analyze.py inspect\|report\|plot\|fixture` | Layouts, clocks, rate, gate, noise floor, fixtures |

Run `make tools` and `make bootstrap` first. Log format: ADR-0010 in `docs/DECISIONS.md`.

## Device

Candidates from `hid-dump` (17 HID devices on this Mac; the rest are power sensors, the keyboard,
trackpad, ambient light, lid angle and the headset jack):

| Product | Transport | Usage | Input report | Class | Notes |
| --- | --- | --- | --- | --- | --- |
| `accel` | SPU | FF00:0003 | 22 B | AppleSPUHIDDevice | **Primary candidate.** Driver says Bosch BMI286 |
| `tp-accel` | FIFO | FF00:0003 | 108 B (report ID 0xC0, 107 B payload) | AppleHIDTransportHIDDevice | In the keyboard/trackpad module |
| `gyro` | SPU | FF00:0009 | 22 B | AppleSPUHIDDevice | Same IMU; rotation, not force |
| `devmotion6` | SPU | FF0C:0005 | 108 B | AppleSPUHIDDevice | Purpose unknown |

`accel` in detail (`hid-dump --name accel`, `ioreg -r -c AppleSPUHIDDriver -l`):

- Matching dictionary (exact keys and values), verified to select exactly this one device both
  with `IOHIDManagerSetDeviceMatching` and with `IOServiceGetMatchingService`:
  `{VendorID: 0x05AC, ProductID: 0x8104, DeviceUsagePage: 0xFF00, DeviceUsage: 0x0003}`.
  `{Transport: "SPU", DeviceUsagePage: 0xFF00, DeviceUsage: 0x0003}` selects it too and does not
  depend on the product ID, which may differ between Mac models (unverified: one Mac so far).
- Do not match on `Product`: `IOHIDDevice::matchPropertyTable` silently ignores it, so
  `{Product: "accel"}` matches every HID device. The tools re-check every key in user space.
- Runtime presence check: `IOServiceGetMatchingService(kIOMainPortDefault,
  IOServiceMatching("IOHIDDevice") + the keys above at the top level)`. Opens nothing.
  200 runs: p50 22 µs, p99 54 µs, max 60 µs when present; p50 43 µs when absent. Cheap enough
  for the main thread.
- IORegistry class and path: `AppleSPUHIDDevice` at
  `IOService:/AppleARMPE/arm-io@10F00000/AppleSoCIO/aop@80600000/AppleASCWrapV6/iop-aop-nub/RTBuddy(AOP)/AOPEndpoint5/AppleSPU@10000008/accel/AppleSPUHIDDevice`,
  behind the always-on processor (AOP). Its event driver is an `AppleSPUHIDDriver` child with
  `model = "BMI286"`, `manufacturer = "Bosch"`, `chip_id = 16`, `dispatchAccel = Yes`,
  `motionRestrictedService = Yes`, `HIDRMDeviceState = "Allowed"`.
- Report descriptor `0600ff0a0300a101150026ff00750895168102c0`: vendor page 0xFF00, usage 3,
  one application collection of 22 opaque 8-bit input bytes, no report ID. The measured payload
  is a four-byte little-endian counter at byte 0, then three little-endian signed 32-bit axes at
  byte 6. The remaining bytes are not interpreted by Keyfeel.
- Reporting state when idle: the device's `ReportInterval` is 8000 µs, but the driver's
  `ReportInterval` is 0 and its `DebugState` counts zero events since boot
  (`_num_events = 0`, `NotificationCount = 0`). The stream is off, and nothing on the system
  consumes it.
- Driver read on 2026-10-24 (`ioreg -a -r -c AppleSPUHIDDriver -l`, read-only, after 34 days of
  uptime): still `ReportInterval = 0` and `_num_events = 0`. The driver lists
  `sensor_rates = "50 100 200 400 800 "` (Hz, a property, not a measurement),
  `calibration_state = 2` and power state 2 of 2, so the sensor is powered but not reporting.
  WindowServer (pid 602) holds an `IOHIDEventServiceUserClient` on it with zero notifications:
  switching the stream on would also feed WindowServer.
- A second driver on the same BMI286, `gyro` (usage FF00:0009, `dispatchGyro = Yes`), is in the
  same state: `ReportInterval = 0`, no events since boot.
- On 2026-10-24, setting only this accelerometer driver's `ReportInterval` to 2500 microseconds
  delivered 6,020 reports in 15.1 seconds (398.6 reports/s). `sensor-log` restored its original
  zero interval; the before/after system-snapshot diff was identical. No reporting-state or
  power-state write was needed.
- Permissions needed (normal user / Input Monitoring / root): see [Permissions](#permissions).

## Permissions

Measured with `sensor-log`, as euid 501 from Visual Studio Code's terminal. The two sensor runs
(`--no-keys --seconds 5`) are saved as `logs/Mac17,3_macos-27.0/probe-user-*.csv`.

| Test | `IOHIDDeviceOpen` | Reports in 5 s |
| --- | --- | --- |
| `accel`, normal user, no Input Monitoring for the host app | 0x00000000 success | 0 |
| `tp-accel`, normal user, no Input Monitoring for the host app | 0x00000000 success | 0 |
| Internal keyboard (usage 0001:0006), normal user, no Input Monitoring | 0xE00002E2 not permitted | - |
| Event tap (`CGEventTapCreate`, listen-only), no Input Monitoring | fails | - |
| `accel` as root (euid 0, Terminal with Input Monitoring), 2026-10-24 | 0x00000000 success | 0 |
| `accel`, normal user, `--wake interval --interval-us 2500`, no Input Monitoring | 0x00000000 success | 6,020 in 15.1 s |

So opening the sensor needs neither root nor Input Monitoring, but opening alone does not start
the stream, and root does not either (`logs/Mac17,3_macos-27.0/probe-root-accel.csv`, 5.1 s, 0
reports, 0 errors; that file is owned by root because `sudo` wrote it). A run with Input
Monitoring granted is not needed: the root run had it. Input Monitoring gates the keyboard
device and the event tap, as expected. System snapshots before and after the root run are
identical apart from the charger being unplugged in between.
Opening the device changed nothing: driver properties were identical before and after.

## Reports

- Report size and byte layout: 22 bytes; a counter at byte 0; `int32x3@6`, little-endian.
- Axis order and sign: report-order x/y/z are preserved. The stationary desk capture is roughly
  `(-0.049, -0.227, -0.974) g`; physical axis naming is unnecessary because force uses the vector
  magnitude relative to its own baseline.
- Units and scale factor: `1 / 65,536 g` per raw unit (the stationary median magnitude snapped
  to `2^-16` with 0.6% error).
- Measured sample rate: 399.0 Hz effective at the 2500 microsecond interval (400 Hz nominal).
- Timestamp source: HID input-report timestamps are mach ticks, with the `125/3` timebase, on the
  same clock as `mach_absolute_time`. Callbacks arrive in bursts, so the core uses the converted
  source timestamp rather than callback arrival time.

## Timing

| Measurement | Median | p95 |
| --- | --- | --- |
| Key-down event → force peak (ms) | 4.28 | 58.91 |
| Key-down event → response onset (ms) | -6.15 | 9.08 |
| Resulting `KF_WINDOW_PRE_NS` / `KF_WINDOW_POST_NS` | 22 / 62 ms | |

The recommended 62 ms post window exceeds the 10 ms product budget, so the shipping config remains
at 5 / 8 ms as a deliberately conservative latency choice, not as a validated force window. The
force-tier path must remain experimental until the long-tail response is understood.

## Separability

Gate: the 10th percentile of hard-tap peaks must exceed the 90th percentile of soft-tap peaks on
the home row and the space bar.

| Surface | Row | Soft p90 (g) | Hard p10 (g) | Separates? |
| --- | --- | --- | --- |
| Desk | Number | 0.0152 | 0.0695 | Yes |
| Desk | Home | 0.0570 | 0.0771 | Yes |
| Desk | Space | 0.0916 | 0.0805 | No |
| Desk | Left edge | 0.0538 | 0.0415 | No |
| Desk | Right edge | 0.0225 | 0.0856 | Yes |
| Lap | Number | 0.0868 | 0.0082 | No |
| Lap | Home | 0.0110 | 0.0523 | Yes |
| Lap | Space | 0.0191 | 0.0106 | No |
| Lap | Left edge | 0.0140 | 0.0097 | No |
| Lap | Right edge | 0.0196 | 0.0198 | Yes |

## Noise floor

| Setting | Peak at rest (g) |
| --- | --- |
| On a desk | p50 0.0112, p95 0.0466, max 0.2088 (10.1 s idle capture) |
| On a lap | p50 0.0034, p95 0.0083, max 0.0210 (10.1 s idle capture) |

## Open questions for the owner

1. **Does the sensor ever deliver data to a plain read-only open?** No, as a normal user or as
   root, on this Mac:
   - **A. Root, read-only: done on 2026-10-24, 0 reports.** The spec's planned root test; it
     changes nothing. The owner ran it (`sudo` is blocked for Claude Code by
     `.claude/settings.json`):

     ```sh
     sudo build/debug/bin/sensor-log --vendor 0x05ac --product 0x8104 --usage-page 0xff00 \
         --usage 3 --no-keys --seconds 5 --label probe=root,device=accel \
         --out "logs/Mac17,3_macos-27.0/probe-root-accel.csv"
     ```

   - **B. Writing `ReportInterval`: works as a normal user.** `--wake interval --interval-us 2500`
     returned 6,020 reports in 15.1 seconds; its cleanup restored the zero interval and the
     before/after snapshots are identical. This is the only setting Keyfeel now writes.
2. **Input Monitoring for key markers.** The desk/lap protocol completed with permission granted
   to the Codex host. Claude Code must never touch the permission database.
3. **Force-quality gate.** Home, number and right-edge keys separate on the desk; space and
   left-edge keys do not. On the lap, only home and right-edge passed; the number-row capture
   also contained a long sensor gap. The product therefore remains “go with limits,” not a
   universal force-tier claim.

## Enabling force detection once the layout is measured

The force path (M2) is enabled on Mac17,3:

1. `KF_SENSOR_LAYOUT_MEASURED` is 1 with the measured `int32x3@6`, `2^-16 g` scale and 400 Hz
   nominal rate. `Tests/Fixtures/sensor/Mac17,3_macos-27.0_accel-rest.csv` anchors the decoder to
   a real report.
2. Force sensing is enabled by default in the app, with an off switch in Sound settings. When
   disabled, sensor start is read-only and does not change the driver's settings. When enabled,
   the app wakes only the accelerometer's report interval while its sensor is running and restores
   the driver afterward. Report timestamps retain the actual 400 Hz cadence through callback
   batching.
3. The existing calibration wizard first measures rest, then asks for soft, hard and normal
   typing. Rest-floor measurement now uses the 95th percentile so one accidental bump cannot
   reject all soft taps.
4. The keyed protocol found that the home gate passes but the space gate does not, and that the
   recommended post window exceeds the 10 ms budget. A release calibration must not claim all
   rows are force-separable until those issues are resolved.

## Recommendation

The hardware path is viable on this Mac: normal-user interval wake produces a stable 400 Hz stream
and restores the driver afterward. The measured force path is useful for selected rows, but the
full M0 quality gate is not met: space/left-edge separation and the post-window budget remain
open. Re-run both the read-only and interval-wake probes on each new Mac model and major macOS
release.
