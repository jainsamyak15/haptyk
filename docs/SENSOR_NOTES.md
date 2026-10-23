# Sensor notes (Milestone 0)

Findings about the MacBook motion sensor. Every value here is measured, never assumed. Fill each
section, then ask the owner for the go/no-go decision.

Spec: docs/SPEC.md, "Milestone 0: sensor discovery (go/no-go gate)".

**Status (2026-10-24): no data without changing the driver.** The sensor is found and opens
both as a normal user and as root, but sends no reports to either: its driver's reporting is
switched off, and only writing the driver's properties could switch it on, which is not
pursued (ADR-0011). Everything that needs samples (layout, scale, rate, timing, separability,
noise) is unmeasured, and the app runs in fallback mode.

## Test machines

| Mac model (`hw.model`) | Model name | Chip | macOS | Date | Notes |
| --- | --- | --- | --- | --- | --- |
| Mac17,3 | MacBook Air | Apple M5 | 27.0 (26A428) | 2026-10-21 | Discovery and permission probes |
| Mac17,3 | MacBook Air | Apple M5 | 27.0 (26A428) | 2026-10-24 | Root probe (test A), driver read |

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
  one application collection of 22 opaque 8-bit input bytes, no report ID. It says nothing
  about fields, so the layout has to be measured (`analyze.py inspect`).
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

So opening the sensor needs neither root nor Input Monitoring, but opening alone does not start
the stream, and root does not either (`logs/Mac17,3_macos-27.0/probe-root-accel.csv`, 5.1 s, 0
reports, 0 errors; that file is owned by root because `sudo` wrote it). A run with Input
Monitoring granted is not needed: the root run had it. Input Monitoring gates the keyboard
device and the event tap, as expected. System snapshots before and after the root run are
identical apart from the charger being unplugged in between.
Opening the device changed nothing: driver properties were identical before and after.

## Reports

Blocked: needs reports. `analyze.py inspect LOG` answers each item once a log has them.

- Report size and byte layout: 22 bytes per report (descriptor); fields TBD
- Axis order and sign: TBD
- Units and scale factor to g: TBD
- Measured sample rate (Hz): TBD (`ReportInterval` 8000 µs is a property, not a measurement)
- Timestamp source and conversion to ns: TBD. `mach_timebase_info` is 125/3 (24 MHz ticks).
  `sensor-log` records each source timestamp raw beside its arrival time, and `analyze.py`
  fits one against the other to measure the units of the HID report timestamp and of
  `CGEventGetTimestamp`.

## Timing

| Measurement | Median | p95 |
| --- | --- | --- |
| Key-down event → force peak (ms) | TBD | TBD |
| Resulting `KF_WINDOW_PRE_NS` / `KF_WINDOW_POST_NS` | TBD | |

## Separability

Gate: the 10th percentile of hard-tap peaks must exceed the 90th percentile of soft-tap peaks on
the home row and the space bar.

| Row | Soft p90 (g) | Hard p10 (g) | Separates? |
| --- | --- | --- | --- |
| Number | | | |
| Top | | | |
| Home | | | |
| Bottom | | | |
| Space | | | |

## Noise floor

| Setting | Peak at rest (g) |
| --- | --- |
| On a desk | TBD |
| On a lap | TBD |

## Open questions for the owner

1. **Does the sensor ever deliver data to a plain read-only open?** No, as a normal user or as
   root, on this Mac. Keyfeel runs in fallback mode (ADR-0011):
   - **A. Root, read-only: done on 2026-10-24, 0 reports.** The spec's planned root test; it
     changes nothing. The owner ran it (`sudo` is blocked for Claude Code by
     `.claude/settings.json`):

     ```sh
     sudo build/debug/bin/sensor-log --vendor 0x05ac --product 0x8104 --usage-page 0xff00 \
         --usage 3 --no-keys --seconds 5 --label probe=root,device=accel \
         --out "logs/Mac17,3_macos-27.0/probe-root-accel.csv"
     ```

   - **B. Writing the driver's properties: not pursued** (ADR-0011). Neither the tools nor
     the app change the sensor driver's state.
2. **Input Monitoring for key markers.** Grant it to the app that runs the tools (Visual Studio
   Code or Terminal) in System Settings → Privacy & Security → Input Monitoring, then reopen it.
   Claude Code must never touch the permission database.
3. **Then run the protocol** (about 20 minutes per surface):
   `Tools/py/.venv/bin/python Tools/py/m0_protocol.py --surface desk`, then `--surface lap`,
   then `Tools/py/.venv/bin/python Tools/py/analyze.py report "logs/Mac17,3_macos-27.0"/*.csv`.
   The report prints these sections as Markdown, ready to paste here.

## Enabling force detection once the layout is measured

The force path (M2) is built and unit-tested against synthetic reports, and stays dormant while
`KF_SENSOR_LAYOUT_MEASURED` is 0. With samples in hand:

1. `analyze.py inspect` on a rest log and a tap log: take the top-ranked layout and its scale.
2. In `Core/include/kf/kf_config.h`, set `KF_SENSOR_SAMPLE_TYPE`, `KF_SENSOR_SAMPLE_OFFSET`,
   `KF_SENSOR_SCALE_G`, `KF_SENSOR_NOMINAL_HZ`, `KF_WINDOW_PRE_NS` and `KF_WINDOW_POST_NS` from
   `analyze.py report`, then set `KF_SENSOR_LAYOUT_MEASURED` to 1.
3. `analyze.py fixture` a rest log into `Tests/Fixtures/sensor/` and add a decode test against
   it (spec: "fixtures recorded in Milestone 0").
4. Fill `App/Resources/calibration-defaults.json` for the model from the report's peaks.
5. `kf-cli --calibrate`, then type soft, normal and hard: the M2 acceptance check.

## Recommendation

No-go for force detection on this Mac without changing the driver. The sensor exists and opens
as a normal user, but sends nothing to a read-only open, as a normal user or as root, so the
spec's gate cannot pass without writing the driver's properties, which is not pursued
(ADR-0011). Recommended: close the gate with the spec's fallback (sounds without force
detection) and re-run the user and root probes on each new Mac model and major macOS release,
in case a later one streams to a plain open. The owner decides.
