# Spec

The source of truth is the owner's Claude Doc **"Keyfeel — macOS Velocity-Sensitive Keyboard
Sounds: Build Spec"**: https://claude.ai/artifact/UiCJx9s4bd34usqpH5s2VS

To update this file: open the doc, export it as Markdown, and replace everything below this line
with the export. Commit it with the message `docs: sync spec`.

---

# Keyfeel — macOS Velocity-Sensitive Keyboard Sounds: Build Spec

Oct 21, 2026 · @samyak jain

## How to use this spec

This spec is the source of truth for building Keyfeel (working name) with Claude Code: build it milestone by milestone and stop at each gate for the owner's review.

Rules for Claude Code:

1. Read the whole spec before writing code, then copy the Safety rules, Hard constraints and Optimization rules sections into `CLAUDE.md` at the repo root.
2. Start with Milestone 0. Build nothing else until the sensor gate passes and the owner approves.
3. Never guess undocumented hardware details such as HID usages, report byte layouts, scale factors or timing. Measure them with the Milestone 0 tools and record them in `docs/SENSOR_NOTES.md`.
4. Values marked **TBD-M0** are placeholders until Milestone 0 measures them. Keep them all in `Core/kf_config.h`.
5. Add no dependencies beyond the Tech stack table without asking the owner. Build no payments, trial, license-key or purchase code: this version is fully unlocked.
6. The shipped app contains no Swift, SwiftUI, Electron, web views or scripting languages. Python is allowed only in `Tools/`.
7. Mark every function that runs on the audio thread or in the sensor callback `/* RT-SAFE */`, and follow the real-time rules in Hard constraints.
8. When the spec is unclear or seems wrong, stop and ask. Record each decision in `docs/DECISIONS.md`.
9. End every milestone with passing tests, its acceptance checks met, and a short report of the measured numbers.

### While building (Claude Code on the owner's Mac)

- Work only inside the repo folder. Create, edit and delete files only there; temporary files go in a git-ignored `build/` or `.tmp/` folder.
- Never run `sudo` without asking first and quoting the exact command. The only expected uses are read-only measurements: `powermetrics` and the Milestone 0 root test.
- Never edit shell profiles (`~/.zshrc`, `~/.bash_profile`), global git config, SSH keys, Xcode preferences, or anything in `/etc`, `/Library`, `/System`, or `~/Library` outside the app's own folders.
- Python packages go in a virtual environment at `Tools/.venv`. No global `pip`, `npm -g` or Homebrew installs without asking.
- No `rm -rf` outside the repo, and no `git push --force`, `git reset --hard` or history rewrites without asking.
- Read-only inspection is fine: `ioreg`, `hidutil list`, `system_profiler`, `sysctl` reads, `pmset -g`, `log show`.
### While running (the customer's Mac)

- The app writes only to its own places: `~/Library/Application Support/Keyfeel/`, its caches folder and its own preferences domain.
- The 128-frame buffer request goes on the app's own output unit only. Milestone 1 verifies that it changes nothing for other apps and does not persist after the app quits.
- No admin password prompts, privileged helpers, kernel or system extensions, launch daemons, or files in `/Library`.
- Launch at login only through `SMAppService`, and only when the user turns it on. It is off by default.
- The app never keeps the Mac awake. Audio stops after the idle timeout so macOS can sleep normally; verify with `pmset -g assertions` that nothing from the app remains 30 s after typing stops.
- Keystrokes pass through untouched. Only Shout Mode, when the user turns it on, adds Shift to letters.
- It asks only for Input Monitoring, plus Accessibility for Shout Mode. It never touches the permission (TCC) database.
- It never injects into, reads files of, or automates other apps, and never scans the disk.
- Updates replace only the app bundle.
- Clean removal: "Remove All Keyfeel Data" in About deletes the app's own folders and preferences and unregisters launch at login. After the app is deleted, nothing of it keeps running.

## Product overview

Keyfeel is a macOS menu bar app that plays mechanical keyboard sounds as you type on a MacBook, matched to how hard each key is hit. It reads the undocumented motion sensor in Apple Silicon MacBooks about 1,000 times a second and picks a soft, medium, hard or slam recording for every keystroke.

- **Users:** MacBook owners who like the sound of mechanical keyboards but type on the built-in keyboard.
- **Hardware:** Apple Silicon MacBook Air and MacBook Pro. Other Macs and external keyboards get sounds without force detection (fallback mode).
- **OS:** macOS 13 Ventura or later, arm64 only.
- **Distribution:** free direct download, not through the Mac App Store. Payments come later and are out of scope here.
- **Positioning:** the fastest, lightest keyboard-sound app on the Mac, fully native, with measurably low battery impact.

Core user flow:

1. The user downloads the DMG, opens the app and sees a short onboarding window.
2. The user grants Input Monitoring, and sounds start immediately.
3. The user picks a sound pack and volume from the menu bar and keeps typing.
4. Optionally, the user runs a 30-second calibration to tune force detection to their typing.

### v1 feature list

Version 1 includes every feature below, all unlocked.

| Feature | What the user gets | Spec section |
| --- | --- | --- |
| Force detection | Four tiers (soft, medium, hard, slam) from the motion sensor at about 1,000 reads/s, crossfaded | Core C modules |
| 16 switch packs | All bundled, each recorded per key group at four forces | Sound pack format |
| Menu bar controls | On/off, volume and pack switching in one click | App shell |
| Live stats | Last keystroke's force, keys today, taps per minute, "harder than N% of users" | App shell, Stats |
| Badges | A force badge from the user's percentile, with a shareable badge card | Stats |
| Sensitivity tuning | Sensitivity slider, calibration wizard and auto-adapt | Core C modules, App shell |
| Community packs | Import `.kfpack` folders or zips | Sound pack format |
| Labs: Big Enter | Slamming Enter plays a boom on top of the switch sound | Labs |
| Labs: Shout Mode | Slamming the keys types letters in capitals | Labs |
| Labs: Force Meter | A small HUD shows each keystroke's force | Labs |
| Auto-pause | Sensor and audio sleep after 30 s without typing | Power state machine |
| Fallback mode | Sounds without force detection on other Macs and external keyboards | Architecture |
| Extras | Key-release sounds, stereo position by key, mute during calls | Core C modules, Power state machine |
| Privacy | No keys, text, apps or URLs collected; anonymous counts only, opt-in | Hard constraints, Stats |
| Feedback | Menu item opens a feedback form or email | App shell |

## Goals and non-goals

Version 1 ships velocity-sensitive keyboard sounds that feel instant and cost almost nothing in CPU, memory and battery.

Goals:

- Keystroke-to-sound feels instant on built-in speakers and wired headphones.
- Four force tiers, blended smoothly, with per-row calibration.
- Realistic sound: several takes per tier, key-down and key-up sounds, stereo position by key.
- Zero CPU use and near-zero wakeups while the user isn't typing.
- Clean fallback when the sensor is missing, blocked or the user types on an external keyboard.
- Auto-mute while the microphone is in use, so calls don't pick up the sounds.
- Every feature in the v1 feature list, plus auto-updates and notarized distribution.

Non-goals for v1:

- Windows, Linux, Intel Macs or iPad.
- Mac App Store distribution.
- Swift, SwiftUI, Electron, Tauri or any web view.
- Payments, trials, license keys, accounts, cloud sync or a pack marketplace.
- Recording, logging or transmitting what the user types.
- Force detection for external keyboards.

## Hard constraints and performance budgets

These budgets are acceptance criteria, measured on an M1 MacBook Air, the slowest supported Mac. Copy this section into `CLAUDE.md`.

| Metric | Budget | Measured with |
| --- | --- | --- |
| Processing time, key event to voice queued (excluding sensor wait) | p99 < 200 µs | `latency-bench` |
| Sensor wait after key-down (POST window) | ≤ 10 ms, exact value TBD-M0 | Milestone 0 analysis |
| Audio output buffer | 128 frames (2.7 ms at 48 kHz) | Engine config |
| Audio callback CPU time | p99 < 10% of the buffer period | Engine counters, Instruments |
| CPU while idle (no typing for 30 s) | 0.0% | Activity Monitor, `top` |
| Wakeups while idle | < 1 per second | `sudo powermetrics --samplers tasks` |
| CPU while typing 600 keys/min | < 1% | Activity Monitor |
| Decoded audio for the active pack | ≤ 8 MB | Pack loader check |
| Memory footprint | < 40 MB | Activity Monitor |
| DMG size with all bundled packs | < 30 MB | Release script |

Real-time rules for the audio thread and the sensor callback:

- No memory allocation or free, no locks or `@synchronized`, no Objective-C message sends, no logging, no file or network I/O and no blocking system calls.
- Threads communicate only through lock-free single-producer/single-consumer (SPSC) ring buffers and C11 atomics.
- All buffers are preallocated when the engine starts or a pack loads.

Privacy rules:

- Key codes are used only to choose a sound, then discarded. They are never stored, logged or transmitted.
- The app never collects typed text, window titles, app names or URLs.
- When macOS Secure Input is active, such as in password fields, the app plays nothing and never tries to work around it.
- Logging of key data exists only in debug builds behind an explicit `KF_DEBUG_KEYS` flag.

## Optimization rules

Every piece of code serves the budgets above in this order: lowest latency, then fewest CPU wakeups, then least memory. These rules add to the real-time rules. Copy this section into `CLAUDE.md`.

Latency:

- The hot path is key event → table lookup → lock-free queue → next audio callback. Nothing on it allocates, locks, logs, calls Objective-C or touches the disk.
- New voices start at the beginning of the very next audio callback: at most 2.7 ms with 128 frames.
- The event tap and sensor run on their own user-interactive threads; the main thread is never on the hot path.
- Packs are decoded, resampled to the device rate and pre-touched (every memory page written once) at load, so playback never resamples or page-faults.

CPU and battery:

- Zero work while idle: no timers, polling loops or periodic wakeups. Only the event tap waits, at no cost.
- Event-driven everywhere: property listeners and notifications, never polling.
- Background work (stats flush, telemetry, percentile download) runs batched at utility or background QoS. Daily jobs use `NSBackgroundActivityScheduler` so macOS picks an efficient time.
- Menu stats refresh only while the menu is open.
- The mixer uses tight loops the compiler auto-vectorizes, or Accelerate (vDSP). Benchmark both and keep the faster. No per-sample branches or function pointers.
- No IIR filters on the audio path. If one is ever added, flush denormals to zero.

Memory:

- Only the active pack is decoded, as 16-bit mono.
- Windows are created when opened and released when closed, so the idle app is just the status item.
- Pools and ring buffers have fixed sizes, allocated once, and never grow.
- Link only the frameworks in Tech stack.

Release build settings:

- Core library: `GCC_OPTIMIZATION_LEVEL = 3`, `LLVM_LTO = YES_THIN`, `DEAD_CODE_STRIPPING = YES`, symbols stripped.
- App: Xcode's default `-Os` for UI code unless profiling shows a reason to change it.
- No debug logging or assertions in Release builds.

Regression checks:

- Every milestone report lists latency-bench p50, p99 and max; idle CPU and wakeups; typing CPU; memory footprint; and the change from the previous milestone.
- A regression above 10% on any budget blocks the milestone until it's fixed or the owner approves it.
- Before release: an 8-hour soak test with `engine-stress` plus real typing. Memory must stay flat, the `leaks` tool must report zero leaks, and Instruments must show zero allocations on the audio and sensor threads after start-up.

## Tech stack

The shipped app is C11 for the core and Objective-C with ARC and AppKit for the UI, built in Xcode for arm64 only.

| Area | Choice | Notes |
| --- | --- | --- |
| Core language | C11 (`-std=c11`) | Key tap, sensor, velocity, audio engine, packs |
| App language | Objective-C with ARC | Menu bar, windows, stats, updates |
| UI framework | AppKit | No SwiftUI, no web views |
| Build | Current stable Xcode, one Xcode project | Core is a static library target |
| Architecture | arm64 only | `ARCHS = arm64` |
| Minimum OS | macOS 13.0 | Needed for `SMAppService` |
| Key capture | CoreGraphics `CGEventTap` | Listen-only by default |
| Sensor and keyboard identity | IOKit `IOHIDManager` | Sensor details come from Milestone 0 |
| Audio output | AudioToolbox, `kAudioUnitSubType_DefaultOutput` | C render callback; follows the default device |
| Audio decoding | AudioToolbox `ExtAudioFile` | Only when a pack loads |
| Mixing math | Accelerate (vDSP) or auto-vectorized C loops | Benchmark both, keep the faster |
| Pack manifest parsing | Foundation `NSJSONSerialization` | In the Objective-C layer; the C core never parses JSON |
| Zip pack import | Built-in `/usr/bin/ditto -x -k` | Extract to a temporary folder, then validate |
| Crash reports | MetricKit (`MXMetricManager`, macOS 12+) | Zero runtime cost; sent only with stats opt-in; PLCrashReporter as fallback |
| Launch at login | ServiceManagement `SMAppService` |  |
| Auto-updates | Sparkle 2 via Swift Package Manager | The only third-party runtime dependency |
| Backend | Small HTTPS API in a separate repo | Anonymous stats, percentiles and opted-in crash reports; Cloudflare Workers with D1 suggested |
| Tests | XCTest | Core logic tested from an Objective-C test target |
| Bug-catching tools | ThreadSanitizer, AddressSanitizer, UndefinedBehaviorSanitizer, Clang Static Analyzer, clang-tidy | Debug and test builds only |
| Pack audio tools | Built-in `afconvert`; Python `soundfile`, `scipy`, `numpy` | In `Tools/.venv` only, never shipped |
| Dev tools | C command-line tools and Python 3 scripts in `Tools/` | Never shipped |

No third-party audio libraries (JUCE, PortAudio, miniaudio): they are cross-platform layers over CoreAudio and would only add size and indirection here.

## Architecture and threading model

Five C modules form one lock-free path from keystroke to speaker; the Objective-C shell only starts, stops and configures them. The diagram is plain text on purpose, so it survives Markdown export for Claude Code.

```
keyboard
   |
   v
[keytap thread]  CGEventTap callback -> kf_key_event (no key code)
   |
   +-- sensor LIVE and built-in key --> pending-key queue (SPSC) --> [sensor thread]
   |                                                                  HID report callback:
   |                                                                  1. decode sample -> accel ring
   |                                                                  2. resolve due keys (velocity.c)
   |                                                                        |
   |                                                                        v
   |                                                               voice queue B (SPSC)
   |                                                                        |
   +-- otherwise, and all key-ups --> voice queue A (SPSC) ---------------+
                                                                            v
                                                     [audio thread] render callback:
                                                     drain A + B -> voice pool -> mixer -> output

[main thread] Objective-C shell: menu, settings, power state machine, pack loading,
              license, telemetry. Talks to the core only through start/stop/config calls and atomics.
```

| Thread | Created by | Runs | Priority | May block? |
| --- | --- | --- | --- | --- |
| Main | AppKit | UI, state machine, pack loading, license, telemetry | Default | Yes, never on the key or audio path |
| Keytap | `keytap.c` (pthread with a CFRunLoop) | Event tap callback, key classification | QoS user-interactive | No |
| Sensor | `sensor.c` (pthread with a CFRunLoop) | HID report callback, ring buffer, velocity resolution | QoS user-interactive | No |
| Audio | CoreAudio | Render callback, voice mixing | Real-time | Never |

Keystroke flow:

1. The keytap callback receives a key-down at `t_key`, converted to nanoseconds of `mach_absolute_time`. It classifies the key into group, row and pan, then discards the key code.
2. If the sensor is LIVE and the key came from the built-in keyboard, it pushes a pending key to the sensor thread. Otherwise it pushes a voice request at default velocity straight to voice queue A.
3. On every HID report, the sensor thread writes the sample to its ring buffer, then resolves each pending key whose window has closed (`t_key + POST <= newest sample time`). It pushes the resulting voice requests to queue B.
4. At the start of each render callback, the audio thread drains both queues, starts voices, mixes and outputs.
5. Key-ups skip the sensor and go straight to queue A with a fixed release gain.
6. When the sensor stops, errors or goes stale, `sensor.c` resolves all pending keys at default velocity, so no keystroke is ever lost.

The sensor thread resolves keys because it is the only thread that knows when the window's samples have arrived. This needs no timers and adds no extra wakeups. All timestamps share one clock; Milestone 0 confirms the units of `CGEventGetTimestamp` and the HID report timestamps.

## Milestone 0: sensor discovery (go/no-go gate)

Milestone 0 proves the motion sensor can be read and can tell soft keystrokes from hard ones. Nothing else gets built until the owner approves its findings.

What is believed, to verify rather than assume: Apple Silicon MacBooks expose a motion sensor through Apple's Sensor Processing Unit (SPU), visible to IOKit as a HID device. Its matching keys, report format, sample rate and required permissions are unknown.

Start with these commands:

```
hidutil list
ioreg -l -w0 | grep -i -E "spu|accel|motion|imu|gyro"
```

Build these tools in `Tools/`:

1. `hid-dump` (C): enumerates every HID device with `IOHIDManager` and NULL matching. For each device it prints vendor ID, product ID, product string, transport, primary usage page and usage, `kIOHIDBuiltInKey`, max input report size, all `DeviceUsagePairs`, and its IORegistry class and path.
2. `sensor-log` (C): opens one device chosen by command-line matching keys and registers an input report callback, preferring `IOHIDDeviceRegisterInputReportWithTimeStampCallback`. It writes CSV rows `t_ns,report_hex` for every report. In the same process, a listen-only `CGEventTap` writes `t_ns,KEYDOWN` and `t_ns,KEYUP` rows, with no key codes.
3. `analyze.py` (Python with numpy and matplotlib): tries candidate layouts (int16, int32 and float32, little-endian, 3 or 6 axes), plots magnitude over time with key markers, and reports peak statistics per keystroke.

Test protocol:

1. Run `hid-dump` and save the candidate devices to `docs/SENSOR_NOTES.md`.
2. For each candidate, log 10 s at rest, then 20 soft taps, 20 hard taps and 30 s of normal typing.
3. Repeat on the space bar, both edges of the keyboard and the number row to measure position effects.
4. Test as a normal user and with `sudo`, with and without Input Monitoring granted, to learn the permission needs.
5. Test with the Mac on a desk and on a lap.

Findings to record in `docs/SENSOR_NOTES.md`:

- Device matching dictionary (exact keys and values) and a cheap runtime presence check.
- Report byte layout, axis order, units or scale factor, and actual sample rate.
- Permissions needed: normal user, Input Monitoring, or root.
- Clock units of `CGEventGetTimestamp` and of HID timestamps, with the conversion to nanoseconds.
- Median and p95 delay from key-down event to peak, which sets the PRE and POST windows.
- Peak magnitude distributions for soft and hard taps, per keyboard row.
- Noise floor at rest, on a desk and on a lap.
- Mac model identifier and macOS version of every test.

Gate decision, made by the owner:

- **Go:** the 10th percentile of hard-tap peaks is above the 90th percentile of soft-tap peaks on the home row and space bar, the sensor works without root, and POST is 10 ms or less.
- **Go with limits:** separation works but needs root, or only some rows separate. The owner decides whether to continue; a privileged helper would add scope.
- **No-go:** the sensor can't be found or can't separate soft from hard. Pivot to a keyboard-sound app without force detection.

## Core C modules

The core is a static library of five C modules with no AppKit dependency, so each one can be driven from command-line tools and tests. Public symbols use the `kf_` prefix, and shared constants live in `Core/kf_config.h`:

```c
#define KF_SAMPLE_RATE_HZ      48000
#define KF_IO_BUFFER_FRAMES    128
#define KF_MAX_VOICES          24
#define KF_ACCEL_RING_SAMPLES  4096           /* ~4 s at 1 kHz */
#define KF_WINDOW_PRE_NS       5000000ULL     /* TBD-M0 */
#define KF_WINDOW_POST_NS      8000000ULL     /* TBD-M0 */
#define KF_BASELINE_NS         50000000ULL    /* 50 ms before the window */
#define KF_SENSOR_STALE_NS     20000000ULL    /* no sample for 20 ms = stale */
#define KF_PENDING_MAX_AGE_NS  40000000ULL    /* resolve at default after 40 ms */
#define KF_QUEUE_A_MAX_AGE_NS  150000000ULL   /* drop voices older than 150 ms */
#define KF_DEFAULT_VELOCITY    0.40f
#define KF_IDLE_TIMEOUT_S      30
```

### keytap.c: keystroke capture

```c
typedef enum { KF_GROUP_ALPHA, KF_GROUP_SPACE, KF_GROUP_ENTER, KF_GROUP_BACKSPACE,
               KF_GROUP_MODIFIER, KF_GROUP_TAB, KF_GROUP_COUNT } kf_key_group;

typedef struct {
    uint64_t t_ns;     /* event time, ns on the mach_absolute_time clock */
    uint8_t  group;    /* kf_key_group */
    uint8_t  row;      /* 0 number row .. 4 space row, 5 function row/other */
    int8_t   pan;      /* -100 left .. +100 right */
    bool     is_down;
    bool     builtin;  /* from the MacBook's own keyboard */
} kf_key_event;        /* deliberately holds no key code */

typedef void (*kf_key_cb)(const kf_key_event *ev, void *ctx);

bool kf_keytap_has_permission(void);      /* CGPreflightListenEventAccess */
void kf_keytap_request_permission(void);  /* CGRequestListenEventAccess */
bool kf_keytap_start(kf_key_cb cb, void *ctx, bool active_mode);
void kf_keytap_stop(void);
```

- Runs on its own pthread with a CFRunLoop. The event mask is key down, key up and flags changed only, never mouse events.
- The tap is listen-only (`kCGEventTapOptionListenOnly`) unless a Labs feature needs `active_mode`.
- On `kCGEventTapDisabledByTimeout` or `kCGEventTapDisabledByUserInput`, re-enable the tap and count the event for diagnostics.
- Modifier keys arrive as flags-changed events; compare with the previous flags to tell down from up.
- Drop auto-repeat events (`kCGKeyboardEventAutorepeat` = 1) unless the setting "Sound on key repeat" is on.
- A static table (Appendix A) maps key code to group, row and pan. The key code is discarded right after the lookup.
- Built-in detection, v1 rule: watch connected keyboards with `IOHIDManager` (usage page 0x01, usage 0x06). With no external keyboard connected, every key is built-in. With one connected, match each event to a HID keyboard event within ±5 ms from a device whose `kIOHIDBuiltInKey` is true; no match means `builtin = false`. Those HID callbacks run on the keytap run loop and record only per-device timestamps.
- If Milestone 0 shows that matching is unreliable, fall back to: any external keyboard connected means no force detection for any key.
- The callback returns within 50 µs.

### sensor.c: motion sensor reader

```c
typedef struct { uint64_t t_ns; float x, y, z; } kf_accel_sample;   /* units: g */

typedef enum { KF_SENSOR_ABSENT, KF_SENSOR_STOPPED, KF_SENSOR_LIVE,
               KF_SENSOR_STALE, KF_SENSOR_DENIED } kf_sensor_state;

bool            kf_sensor_probe(void);      /* device present? cheap, main thread */
bool            kf_sensor_start(void);      /* opens device, starts its thread */
void            kf_sensor_stop(void);       /* resolves pending keys, closes device */
kf_sensor_state kf_sensor_state_now(uint64_t now_ns);   /* any thread */
bool            kf_sensor_push_pending(const kf_key_event *ev);  /* keytap thread only */
```

- Device matching, report decoding and scaling come from `SENSOR_NOTES.md`. Decoding lives in one function, `kf_sensor_decode_report()`, unit-tested with reports recorded in Milestone 0.
- The report callback decodes one sample, writes it to the ring buffer, then resolves pending keys. It does nothing else.
- The ring buffer is private to the sensor thread, so it needs no synchronization. Only the pending-key queue and the state are shared.
- The state is an atomic readable from any thread. LIVE means a sample arrived within `KF_SENSOR_STALE_NS`.
- On stop, device removal or error, every pending key resolves at `KF_DEFAULT_VELOCITY`. A pending key older than `KF_PENDING_MAX_AGE_NS` also resolves at default.
- Keep counters for dropped samples and the measured sample rate, for diagnostics.

```c
/* RT-SAFE: runs after each new sample, on the sensor thread */
while (pending_peek(&k) && k.t_ns + KF_WINDOW_POST_NS <= newest_t_ns) {
    float v = kf_velocity_estimate(&ring, k.t_ns, k.row, &calibration);
    if (v < 0.0f) v = KF_DEFAULT_VELOCITY;      /* not enough data */
    kf_emit_keystroke(&k, v, KF_QUEUE_B);       /* builds voice requests */
    pending_pop();
}
```

### velocity.c: force estimation

```c
typedef struct {
    float noise_floor_g;   /* peak at rest */
    float soft_g;          /* typical soft-tap peak, maps to v = 0 */
    float slam_g;          /* typical slam peak, maps to v = 1 */
    float row_gain[6];     /* compensates distance from the sensor */
    bool  auto_adapt;
} kf_calibration;

float kf_velocity_estimate(const kf_accel_ring *r, uint64_t t_key_ns,
                           uint8_t row, const kf_calibration *cal);  /* 0..1 or -1 */
void  kf_tier_weights(float v, int *lo, float *w);
```

Estimation:

1. Baseline: the mean of each axis over `[t_key - PRE - 50 ms, t_key - PRE)`. This removes gravity and the Mac's tilt.
2. Window: samples in `[t_key - PRE, t_key + POST]`. For each, take the magnitude of the sample minus the baseline.
3. Peak: the largest magnitude in the window, multiplied by `row_gain[row]`.
4. Normalize on a log scale: `v = (ln(peak) - ln(soft_g)) / (ln(slam_g) - ln(soft_g))`, clamped to 0..1. A peak below `noise_floor_g` gives `v = 0`.
5. Return -1 when the window holds fewer than half the expected samples.

Tier blending across four tiers (0 soft, 1 medium, 2 hard, 3 slam):

```c
void kf_tier_weights(float v, int *lo, float *w) {
    float x = v * 3.0f;
    *lo = x >= 3.0f ? 2 : (int)x;
    *w  = x - (float)*lo;           /* share of tier lo + 1 */
}
```

Each keystroke starts two voices, tiers `lo` and `lo + 1`, with equal-power gains `sqrt(1 - w)` and `sqrt(w)`. Skip a voice whose gain is below 0.05. Overall gain also rises gently with force: `0.7 + 0.3 * v`.

Auto-adapt, when on: keep the last 512 row-normalized peaks in a preallocated array. Every 64 keystrokes, move `soft_g` toward the 10th percentile and `slam_g` toward the 97th percentile with a smoothing factor of 0.1. Never move either outside 0.5x to 2x of the calibrated value.

Calibration wizard math (UI in the app shell):

1. Measure the noise floor for 3 s at rest.
2. The user types a given sentence softly, then hard, then normally, about 40 keys each.
3. `soft_g` is the median soft peak and `slam_g` the 95th percentile hard peak. `row_gain[r]` is the overall median hard peak divided by row r's median hard peak, clamped to 0.5 to 2.0.
4. Save as JSON in user defaults under `calibration.v1`.

Defaults per Mac model come from `Resources/calibration-defaults.json`, keyed by `hw.model` from `sysctl` (for example `Mac14,2`), and filled from Milestone 0 and beta measurements.

### engine.c: audio engine

```c
typedef struct {
    uint64_t t_ns;      /* keystroke time, used to drop stale requests */
    uint8_t  group, tier, is_up;
    float    gain;      /* 0..1 */
    float    pan;       /* -1..+1 */
} kf_voice_req;

bool kf_engine_init(void);
bool kf_engine_start(void);              /* returns once the output unit runs */
void kf_engine_stop(void);
bool kf_engine_is_running(void);
void kf_engine_set_volume(float v);      /* atomic, smoothed in the callback */
void kf_engine_set_muted(bool muted);    /* atomic */
void kf_engine_set_pack(kf_pack *p);     /* main thread, safe handoff */
void kf_emit_keystroke(const kf_key_event *k, float v, kf_queue q);  /* builds and queues requests */
```

- Output unit: `kAudioUnitSubType_DefaultOutput`, Float32, non-interleaved, 2 channels, at the device's nominal sample rate.
- Request a 128-frame buffer with `kAudioDevicePropertyBufferFrameSize` on the current default device. Accept what the device grants and record it for diagnostics.
- Listen for default-device and sample-rate changes. On a change: stop, reconfigure, re-resample the pack on the main thread, restart.
- Voice pool: `KF_MAX_VOICES` fixed slots. When full, steal the oldest voice with a 2 ms fade-out.
- The audio thread drops any request older than `KF_QUEUE_A_MAX_AGE_NS`, so a sound delayed by audio start-up never plays late.
- Take choice: random among the tier's takes, never the same take twice in a row. Use a xorshift generator, not `rand()`.
- Pan: constant power, `L = cos((pan + 1) * pi/4)`, `R = sin((pan + 1) * pi/4)`, scaled by the Stereo width setting (0 = mono).
- Master: smoothed volume, then a soft clipper above 0.8 so many voices never hard-clip.
- Samples are stored as 16-bit mono and converted to float while mixing.
- Pack handoff: the main thread builds the new pack fully, then publishes its pointer atomically. The audio thread swaps it in at the start of the next callback and bumps an epoch counter. The main thread frees the old pack only after the epoch changes, polling every 10 ms for at most 1 s. Voices on the old pack fade out over 5 ms.
- `kf_engine_start` records the time from call to first callback, for diagnostics and the idle-timeout choice.

```c
/* RT-SAFE */
static OSStatus render(void *ctx, AudioUnitRenderActionFlags *flags,
                       const AudioTimeStamp *ts, UInt32 bus,
                       UInt32 frames, AudioBufferList *out) {
    kf_engine *e = ctx;
    swap_pack_if_published(e);
    drain_queues(e);                      /* queue A and queue B */
    if (e->active_voices == 0) {          /* silent: cheapest path */
        *flags |= kAudioUnitRenderAction_OutputIsSilence;
        for (UInt32 i = 0; i < out->mNumberBuffers; i++)
            memset(out->mBuffers[i].mData, 0, out->mBuffers[i].mDataByteSize);
        return noErr;
    }
    mix_voices(e, out, frames);
    return noErr;
}
```

The fifth module, `packs.c`, is specified in Sound pack format.

## Sound pack format

A sound pack is a `.kfpack` folder holding a `pack.json` manifest and mono audio files, decoded fully into RAM when the user selects it.

```
NK-Cream.kfpack/
  pack.json
  samples/
    alpha_down_soft_1.wav
    alpha_down_soft_2.wav
    ...
    space_up_1.wav
```

```json
{
  "format": 1,
  "id": "com.keyfeel.nk-cream",
  "name": "NK Cream",
  "switch_type": "linear",
  "author": "Keyfeel",
  "license": "proprietary",
  "version": "1.0.0",
  "groups": {
    "alpha": {
      "down": {
        "soft":   ["samples/alpha_down_soft_1.wav", "samples/alpha_down_soft_2.wav"],
        "medium": ["samples/alpha_down_medium_1.wav"],
        "hard":   ["samples/alpha_down_hard_1.wav"],
        "slam":   ["samples/alpha_down_slam_1.wav"]
      },
      "up": ["samples/alpha_up_1.wav", "samples/alpha_up_2.wav"]
    },
    "space": { "down": { "medium": ["samples/space_down_medium_1.wav"] } }
  }
}
```

Rules:

- Group keys are `alpha`, `space`, `enter`, `backspace`, `modifier` and `tab`. Only `alpha` with at least one down sample is required. Every pack also records its source URL, license and required attribution, and marks any tier generated rather than recorded as derived.
- `switch_type` is one of `linear`, `tactile`, `clicky`, `topre`, `buckling` or `other`.
- Files are WAV or CAF, PCM (16 or 24-bit) or ALAC, mono, 44.1 or 48 kHz, each 1.5 s or shorter. Bundled packs use CAF with ALAC to keep the DMG small.
- The loader decodes with `ExtAudioFile` into 16-bit mono at the output device's sample rate.
- Fallbacks, in order: a missing tier uses the nearest tier in the same group (ties go to the softer one); a missing group uses `alpha`; a missing `up` list means no release sound.
- Limits: at most 400 files and 8 MB decoded. Otherwise reject the pack with a readable error.
- Bundled packs live in `Keyfeel.app/Contents/Resources/Packs/`, user packs in `~/Library/Application Support/Keyfeel/Packs/`.
- Import accepts a `.kfpack` folder or a `.zip`. Unzip with the built-in ditto tool into a temporary folder, then validate everything before copying: the JSON schema, paths that stay inside the pack, no symlinks and the size limits. Never execute anything from a pack.

```c
typedef struct {
    uint8_t     group;     /* kf_key_group */
    uint8_t     is_up;     /* 0 key-down, 1 key-up */
    uint8_t     tier;      /* 0 soft .. 3 slam; ignored for key-up */
    bool        derived;   /* generated tier, not a real recording */
    const char *path;      /* absolute path to one audio file */
} kf_pack_file;

typedef struct kf_pack kf_pack;   /* opaque: decoded int16 buffers plus an index */

kf_pack *kf_pack_load(const kf_pack_file *files, size_t count, double device_rate,
                      char *err, size_t errlen);
void     kf_pack_free(kf_pack *p);
```

`KFPackLibrary` (Objective-C) parses and validates `pack.json` with `NSJSONSerialization`, applies the fallback rules, and hands `kf_pack_load` a flat file list. The C core never parses JSON. Loading runs on a background queue, never on a real-time thread.

Test pack: Claude Code cannot record real switches, so `Tools/make_test_pack.py` synthesizes a complete pack: every group, four tiers, three takes each and release sounds. It builds them from short filtered noise bursts and resonant clicks, louder and brighter as force rises. The resulting "Test Click" pack ships in debug builds only.

Real packs come from licensed sources (Appendix B) and processed by `Tools/build_pack.py`. It splits takes on silence, trims, adds a 2 ms fade-in, normalizes per tier, encodes to CAF/ALAC with the built-in afconvert tool and writes `pack.json`.

### Bundled packs

All 16 packs ship in v1, bundled and unlocked. Every file comes from a source whose license allows commercial use inside an app (Appendix B); never copy audio files from another app.

| Pack | Switch type |
| --- | --- |
| Alpaca | Linear |
| Biscuit | Tactile |
| Blue Alps | Clicky |
| Buckling Spring | Buckling spring |
| Cherry MX Black PBT | Linear |
| Cherry MX Blue PBT | Clicky |
| Cherry MX Brown PBT | Tactile |
| Cherry MX Red PBT | Linear |
| EG Crystal Purple | Tactile |
| Gateron Black Ink | Linear |
| Gateron Red Ink V2 | Linear |
| Holy Panda | Tactile |
| Kailh Box Navy | Clicky |
| NK Cream | Linear |
| Topre PBT | Topre |
| Turquoise Tealios | Linear |

Until the recordings exist, debug builds use the Test Click pack for every slot. Release builds include only finished packs, and the pack list shows them in this order, with the default pack chosen by the owner. Name each pack after what the recording really is, and drop or rename any of the 16 that can't be sourced legally. The About window lists every attribution a license requires.

## App shell (Objective-C)

The app is an `LSUIElement` agent with no Dock icon: a status bar item, a settings window created on demand, and a first-run onboarding window.

| Class | Responsibility |
| --- | --- |
| `KFAppDelegate` | Launch, owns the controllers, wires core callbacks |
| `KFPowerController` | Power state machine; starts and stops core modules |
| `KFStatusMenuController` | `NSStatusItem`, menu building, live stats while the menu is open |
| `KFVolumeMenuView` | Custom `NSView` with a slider inside an `NSMenuItem` |
| `KFOnboardingWindowController` | Permissions, stats choice, first sound test |
| `KFSettingsWindowController` | Tabbed settings window, released when closed |
| `KFCalibrationController` | Calibration wizard |
| `KFPackLibrary` | Lists bundled and user packs; import and delete |
| `KFStatsController` | Local stats, percentile lookup, badge and badge card |
| `KFTelemetry` | Daily anonymous upload, when opted in |
| `KFPreferences` | Typed wrapper around `NSUserDefaults` |

Status item:

- Template (monochrome) icon with three states: on, off (dimmed) and needs attention (a dot when Input Monitoring is missing).
- Build menu items once and refresh their values in `menuWillOpen:`.

Menu, top to bottom:

1. Header with an on/off switch.
2. Volume slider.
3. Sound pack list with a checkmark on the current pack, then "More packs…", which opens Settings → Packs.
4. Force status: "Force: Live", "Force: Off (external keyboard)", "Force: Not available on this Mac" or "Force: Calibrate for best results".
5. Stats: the last keystroke's force, keys today, taps per minute, "harder than N% of users" and the current badge. A 0.5 s timer updates them only while the menu is open.
6. Labs submenu.
7. Calibrate…, Settings… and Check for Updates…
8. Send Feedback… and Quit (⌘Q).

Settings tabs (`NSTabViewController`):

- **General:** launch at login (off by default), sound on key release, stereo width, sound on key repeat, mute while the microphone is in use, mute when typing on an external keyboard, idle timeout (advanced).
- **Sound:** volume, pack, sensitivity slider (shifts `soft_g` and `slam_g` together), auto-adapt and Run calibration….
- **Packs:** table of packs (name, switch type, bundled or user), Import…, Reveal in Finder, Delete for user packs, and a preview button that plays all four tiers.
- **Labs:** one toggle per feature with a one-line description and its permission status.
- **Stats:** today's numbers, a 7-day force histogram, percentile and badge, Share Badge…, Reset Stats and the anonymous-stats opt-in.
- **About:** version, acknowledgements including Sparkle's license, privacy policy link, sound attributions, Export Diagnostics and Remove All Keyfeel Data.

Onboarding, shown on first launch and whenever Input Monitoring is missing:

1. Welcome screen with a 10-second explanation.
2. Input Monitoring: one button calls `CGRequestListenEventAccess()`, another opens `x-apple.systempreferences:com.apple.preference.security?Privacy_ListenEvent`. Poll `CGPreflightListenEventAccess()` once per second only while this window is open. When granted, retry the tap; if it still fails, offer "Relaunch Keyfeel".
3. Anonymous stats: a plain opt-in checkbox, off by default.
4. A test field where the user types and sees the detected tier live.

Accessibility permission is requested only when the user turns on a Labs feature that needs it, through `AXIsProcessTrustedWithOptions` with the prompt option.

Preference keys: `enabled`, `volume`, `packId`, `releaseSounds`, `stereoWidth`, `repeatSounds`, `muteWhenMicActive`, `muteExternalKeyboard`, `idleTimeoutS`, `sensitivity`, `autoAdapt`, `calibration.v1`, `labs.*`, `telemetryOptIn`, `onboardingDone`.

Export Diagnostics writes a text file with the app version, Mac model, macOS version, sensor state, measured sample rate, audio device name, granted buffer size, engine start time and counters (tap timeouts, dropped samples, callback overruns). It never includes key data.

## Power state machine

The sensor stays closed and the audio unit stopped whenever the user isn't typing, so idle cost is zero. `KFPowerController` owns the state on the main thread.

| State | Key tap | Sensor | Audio unit |
| --- | --- | --- | --- |
| `DISABLED` (sounds turned off) | Stopped | Closed | Stopped |
| `NO_PERMISSION` | Stopped, retried when granted | Closed | Stopped |
| `IDLE` | Running | Closed | Stopped |
| `ACTIVE` | Running | Open, if present | Running |

| From | Event | To | Action |
| --- | --- | --- | --- |
| `IDLE` | Key-down | `ACTIVE` | Start audio and sensor in parallel; this key plays at default velocity once audio runs |
| `ACTIVE` | No key for the idle timeout (default 30 s) | `IDLE` | Stop sensor (resolving pending keys), stop audio |
| `ACTIVE` | System sleep, screen lock or session switched out | `IDLE` | Same as above |
| `ACTIVE` | Output device or sample rate changes | `ACTIVE` | Restart audio with the new format; re-resample the pack |
| Any | User turns sounds off | `DISABLED` | Stop everything |
| `DISABLED` | User turns sounds on | `IDLE` | Start the key tap |
| Any | Input Monitoring revoked or tap creation fails | `NO_PERMISSION` | Show the badge; offer onboarding |
| `NO_PERMISSION` | Permission granted | `IDLE` | Start the key tap, or offer a relaunch |

Implementation notes:

- The keytap callback triggers IDLE → ACTIVE with an atomic flag and a single `dispatch_async` to the main thread on that first key only, never on every key.
- The keytap thread writes the last key time to an atomic `uint64_t`. While ACTIVE, a main-thread timer fires every half idle timeout to check it. The timer is invalidated in IDLE.
- Watch sleep and session changes with `NSWorkspaceWillSleepNotification` and `NSWorkspaceSessionDidResignActiveNotification`, and screen lock with the distributed notification `com.apple.screenIsLocked`. Verify the lock notification still fires on the current macOS.
- Mic-in-use mute: a property listener for `kAudioDevicePropertyDeviceIsRunningSomewhere` on the default input device, re-registered when the default input changes. No polling. The app never opens the microphone; verify in M3 that reading this property triggers no microphone permission prompt.
- Muting only sets the engine's muted flag. It does not change state, so unmuting is instant.

## Labs (ships in v1)

Labs are force-triggered extras that ship in v1 as toggles in the Labs submenu, off by default. Use original feature names, and a boom sound that is either recorded by the owner or properly licensed; never copy a competitor's audio.

| Feature | Behavior | Needs |
| --- | --- | --- |
| Big Enter | Enter hit at velocity 0.85 or more also plays a deep original "boom" sample | Listen-only tap |
| Shout Mode | When the rolling force of the last 3 keystrokes is 0.8 or more, letters are typed as capitals | Active tap and Accessibility |
| Force Meter | A small floating HUD shows each keystroke's force for 1 s | Nothing extra |

Shout Mode rules:

- The active tap never waits for sensor data. It decides from the rolling force of previous keystrokes, which the sensor thread publishes as an atomic float.
- It only adds the Shift flag to letter keys a–z. It never touches other keys, shortcuts with ⌘, ⌃ or ⌥, or anything during Secure Input.
- The tap runs in active mode only while Shout Mode is on; otherwise it stays listen-only.
- If macOS disables the tap for a timeout, re-enable it. After 3 timeouts within a minute, turn Shout Mode off and tell the user.

Labs sounds live in `Resources/Labs/` and use the same audio engine, as an extra voice request from the sensor thread.

## Stats, telemetry and privacy

Local stats are counts only, and anything sent to the server is anonymous, sent once a day, and only with the user's opt-in.

Local stats, stored in `~/Library/Application Support/Keyfeel/stats/YYYY-MM-DD.json` and kept for 90 days:

- Keystroke count, a 10-bucket force histogram, active typing minutes and peak keys per minute.
- Updated with atomics on the hot path, written to disk at most every 5 minutes and on quit.
- A "Reset stats" button deletes them and regenerates the install ID.

Telemetry, only when opted in:

- One POST per day, batching any unsent days, retried at the next launch. Never per keystroke.
- Uploads feed the shared percentile table described below.

```json
{
  "install_id": "random UUID",
  "app_version": "1.0.0",
  "mac_model": "Mac14,2",
  "macos": "26.1",
  "day": "2026-10-21",
  "keys": 4812,
  "force_hist": [120, 340, 900, 1200, 1000, 700, 330, 150, 52, 20],
  "force_mode": "live"
}
```

Crash reports: at launch the app subscribes to MetricKit (`MXMetricManager`, macOS 12+), which hands over crash and hang diagnostics from earlier sessions at no runtime cost. They go out with the daily upload only when the user has opted in; otherwise they stay on the Mac for 30 days and are included in Export Diagnostics. Verify in M5 that MetricKit delivers payloads to apps distributed outside the App Store; if it doesn't, use PLCrashReporter, which does nothing until a crash happens. Crash reports never contain key data.

The privacy policy (final text by the owner) states what is collected (above), what never is (keys, text, apps, windows, URLs), that the user can opt out anytime in Settings, and how to delete local stats.

### Percentile and badges

Every user sees "harder than N% of users", even without opting in. The app downloads the daily percentile table with `GET /v1/stats/percentiles`, which sends nothing about the user, and compares locally. Cache the table for 24 hours.

- Force for comparison is the raw, row-compensated peak in g, not the calibrated 0–1 value, because calibration differs per user. The 10 histogram buckets use fixed log-spaced edges, the same for everyone (edges TBD-M0).
- Tables are per Mac model when the backend has at least 200 users for that model, otherwise global.
- The user's force is their median over the last 7 days, once they have 500 keystrokes. Before that the menu shows "Keep typing to earn your badge". With no network, hide the line.

| Badge | User's force percentile |
| --- | --- |
| Feather | Below 25th |
| Steady | 25th to 60th |
| Hammer | 60th to 90th |
| Thunder | Top 10% |

Share Badge… draws a 1200 × 630 PNG card (badge, percentile, current pack) with AppKit and opens the macOS share sheet. Nothing leaves the Mac unless the user shares it.

## Backend API

The app talks to one small backend, only for anonymous stats and the percentile table. This version has no payments, trials or license keys.

| Method and path | Body | Response | Called by |
| --- | --- | --- | --- |
| `POST /v1/telemetry/daily` | Array of telemetry payloads | `{ok}` | Opted-in users, once a day |
| `POST /v1/diagnostics` | Array of MetricKit crash and hang payloads, with app version and Mac model | `{ok}` | Opted-in users, with the daily upload |
| `GET /v1/stats/percentiles?model=<hw.model>` | None | `{day, model, edges, percentiles}` | Every user, at most once a day |

All requests use HTTPS with a 10 s timeout and run off the main thread. No network call ever happens on the key or audio path. The backend lives in a separate repo; Cloudflare Workers with D1 is enough.

## Updates, signing and release

Releases are Developer ID–signed, notarized DMGs, updated through Sparkle 2.

Sparkle:

- Add it with Swift Package Manager and use `SPUStandardUpdaterController` from Objective-C.
- `Info.plist` keys: `SUFeedURL` (the owner's appcast URL), `SUPublicEDKey` and `SUEnableAutomaticChecks = YES`, checking once a day.
- Generate the EdDSA keys once with Sparkle's `generate_keys`. The private key never enters the repo.

Signing and runtime:

- Developer ID Application certificate from the Apple Developer Program.
- Hardened Runtime on. App Sandbox off, because the event tap and the sensor need it off.
- Entitlements start empty. Add only what Milestone 0 proves necessary, and record why in `DECISIONS.md`.
- `Info.plist`: `LSUIElement = YES`, `LSMinimumSystemVersion = 13.0`, the owner's `CFBundleIdentifier` and a copyright line.

`Tools/release.sh` runs these steps:

1. `xcodebuild archive` in Release for arm64.
2. Export with method `developer-id`.
3. Zip the app, `xcrun notarytool submit --wait`, then `xcrun stapler staple`.
4. Build the DMG, sign it, notarize it and staple it.
5. Run Sparkle's `generate_appcast` over the releases folder.
6. Print the DMG size and fail if it is over 30 MB.

Notarization credentials come from a keychain profile created with `xcrun notarytool store-credentials`, never from the repo.

## Testing and performance verification

A milestone is done only when its unit tests pass, its performance budgets are measured and met, its rows of the manual test matrix pass, and the sanitizer and analyzer runs are clean.

Unit tests (XCTest, calling the C core from Objective-C):

- SPSC queue: ordering, full and empty, wraparound, and a 10-million-item producer/consumer stress test.
- `kf_sensor_decode_report`: fixtures recorded in Milestone 0.
- Velocity: synthetic samples with known peaks map to the expected `v`; baseline removal; too little data returns -1; tier weights at v = 0, 0.33, 0.5 and 1.
- Pack loader: a valid pack, every fallback rule, malformed JSON, path traversal (`../`), oversize packs and unsupported formats.
- Mixer: voice stealing, the silence flag, output kept within ±1.0 by the clipper, and the pan law.
- Key map: every key code in Appendix A maps to a group, row and pan.

Tools:

- `latency-bench`: pushes 100,000 synthetic key events through classification, velocity and queueing; reports p50, p99 and max processing time.
- `engine-stress`: 30 voices per second for 10 minutes while switching packs every 10 s; reports callback overruns and memory growth.
- Debug counters exposed in diagnostics: longest callback, overruns, tap timeouts and dropped sensor samples.

Bug-catching builds (debug and test only, never shipped):

- **ThreadSanitizer** on every run of the SPSC queue tests and `engine-stress`. Lock-free code must be TSan-clean.
- **AddressSanitizer and UndefinedBehaviorSanitizer** on the full unit-test suite.
- **Clang Static Analyzer** (Xcode's Analyze) and **clang-tidy** with the bugprone, performance and concurrency checks, at zero warnings for every milestone.
- **RealtimeSanitizer** (`-fsanitize=realtime`, upstream LLVM) on the audio and sensor callbacks, optional: it catches allocations and locks inside real-time code, but Apple's compiler may not support it yet, and installing another compiler needs the owner's approval.

Before each release, check Activity Monitor (CPU, memory, energy impact) idle and while typing, run `sudo powermetrics --samplers tasks -i 5000` for idle wakeups, and confirm zero audio-thread allocations with Instruments (Time Profiler and Allocations).

| Area | Cases |
| --- | --- |
| Macs | M1 Air, M2 Air, an M3 or M4 Pro, the newest model available through beta testers |
| Output | Built-in speakers, wired headphones, USB DAC, AirPods (expect extra latency), HDMI display |
| Keyboards | Built-in only; external connected but typing on built-in; typing on external |
| System | Password field stays silent; sleep and wake mid-typing; lock screen; fast user switching; Low Power Mode |
| Changes | Switch output device, pack or volume while typing; mute toggle |
| Permissions | Fresh install; Input Monitoring denied then granted; revoked while running |
| Load | 1,000 keys/min bursts; held keys (auto-repeat); many apps open |
| Calls | Start a FaceTime or Zoom call: sounds mute; end the call: sounds return |

## Milestones

Milestones run strictly in order. Each ends with a short report to the owner with the measured numbers, and the next starts only after approval.

1. **M0, sensor discovery (gate).** Tools, protocol and findings as in Milestone 0.
   - Done when `SENSOR_NOTES.md` answers every listed question and the owner has made the go/no-go call.
2. **M1, core sound pipeline (command line).** `keytap.c`, `keymap.c`, `engine.c`, `packs.c`, the test pack generator, and `kf-cli`, which plays the test pack at default velocity while you type.
   - Done when `latency-bench` p99 is under 200 µs, the silence path sets its flag, idle CPU is 0.0% with the engine stopped, and unit tests pass.
3. **M2, force detection (command line).** `sensor.c`, `velocity.c`, pending-key resolution, tier blending, calibration math and `kf-cli --calibrate`.
   - Done when soft, normal and hard typing produce clearly different tiers on the test Mac, a missing sensor falls back cleanly, and unit tests pass.
4. **M3, menu bar app.** App shell, power state machine, onboarding, settings, calibration wizard, mic-in-use mute and launch at login.
   - Done when every budget in Hard constraints is met inside the app and the System, Changes and Permissions rows of the test matrix pass.
5. **M4, sound packs.** Pack library, import, `build_pack.py`, and all 16 packs from licensed sources (Appendix B), one of each switch type first.
   - Done when all 16 packs are bundled, each with a SOURCES.md and a license allowing commercial use, the owner approves their sound, and all pack tests pass.
6. **M5, shipping pipeline.** Sparkle, opt-in telemetry, percentile and badges, crash reporting, Export Diagnostics, the release script and a notarized DMG.
   - Done when a fresh Mac can download, install, show its percentile and badge, and auto-update.
7. **M6, Labs and polish.** Labs features, icon states, accessibility labels on every control, and a beta on several Mac models.
   - Done when the full test matrix passes on at least 3 Mac models.

## Repo layout and coding conventions

One Xcode project holds the C static library, the app, a test target and the command-line tools.

```
keyfeel/
  CLAUDE.md                  rules for Claude Code, copied from this spec
  Keyfeel.xcodeproj
  Core/                      C11 static library, no AppKit
    kf_config.h
    kf_spsc.h                lock-free SPSC ring buffer (header-only)
    keytap.c/.h
    keymap.c/.h              key code -> group, row, pan
    sensor.c/.h
    velocity.c/.h
    engine.c/.h
    packs.c/.h
  App/                       Objective-C with ARC
    KFAppDelegate.m
    KFPowerController.m
    Menu/  Settings/  Onboarding/  Calibration/
    Stats/  Telemetry/  Preferences/
    Resources/               assets, Packs/, Labs/, calibration-defaults.json
  Tests/                     XCTest
  Tools/
    hid-dump.c  sensor-log.c  latency-bench.c  engine-stress.c  kf-cli.c
    analyze.py  make_test_pack.py  build_pack.py
    release.sh
  docs/
    SENSOR_NOTES.md  DECISIONS.md  PRIVACY.md
```

Conventions:

- **C:** C11 with `-Wall -Wextra -Wpedantic -Werror` and `clang-format` (LLVM style, 100 columns). Public symbols use `kf_`; everything else is file-static. Use fixed-width integer types.
- **Objective-C:** ARC, the `KF` class prefix, nullability annotations on every header, no Swift files.
- **Real-time code:** mark functions `/* RT-SAFE */`, keep them in clearly separated sections, and review every change to them against the real-time rules.
- **Errors:** core functions return `bool` or error codes and never abort in release builds. Assertions run in debug builds only.
- **Logging:** `os_log` with the app's subsystem, never on real-time threads and never with key data.
- **Commits:** one logical change each, with a message saying what and why. Tag the commit that finishes each milestone.

## Open decisions for the owner

These choices are still open; Claude Code uses the default shown until the owner decides.

| Decision | Default until decided |
| --- | --- |
| Final app name and bundle ID | "Keyfeel", `com.example.keyfeel` |
| Telemetry opt-in or opt-out | Opt-in, off by default |
| Sound source for each pack | Commissioned recordings; CC0, CC-BY or paid libraries as fallback |
| Default pack on first launch | The first finished linear pack |
| Badge names | Feather, Steady, Hammer, Thunder |
| Minimum macOS | 13.0 |
| Privileged helper if the sensor needs root | Out of scope unless the owner approves after M0 |
| When to add payments | After v1, in a separate spec |

## Appendices

### A. Key mapping rules

Claude Code generates the full table in `Core/keymap.c` from the `kVK_` key code constants in `<Carbon/Carbon.h>`, using these rules for an ANSI MacBook layout. Pan runs from -1 (left edge) to +1 (right edge) and is stored as -100 to +100.

| Keys | Group | Row | Pan |
| --- | --- | --- | --- |
| Esc, F1–F12 | alpha | 5 | By column |
| Backtick, 1–0, minus, equals | alpha | 0 | By column |
| Delete, Forward Delete | backspace | 0 | +0.9 |
| Tab | tab | 1 | -0.9 |
| Q–P, brackets, backslash | alpha | 1 | By column |
| Caps Lock | modifier | 2 | -0.95 |
| A–L, semicolon, quote | alpha | 2 | By column |
| Return | enter | 2 | +0.9 |
| Left Shift, Right Shift | modifier | 3 | -0.9, +0.85 |
| Z–M, comma, period, slash | alpha | 3 | By column |
| fn, Control, Option, Command | modifier | 4 | By position |
| Space | space | 4 | 0 |
| Arrow keys | alpha | 4 | +0.8 to +0.95 |

"By column" means the key's horizontal center on the physical keyboard, scaled from -1 to +1. Key codes not in the table (ISO and JIS extras) map to alpha, row 2, pan 0.

### B. Sourcing sounds (for the owner)

- **Never:** copy or extract audio from Haptyk or any other app, or use clips from YouTube or TikTok switch tests without written permission. Those are someone else's recordings.
- **Commissioned recordings (best):** hire a sound recordist or a keyboard enthusiast who already owns the switches, for example through Fiverr, Upwork or r/MechanicalKeyboards. Send them the recording spec below and get a written license granting you commercial rights. This is the only route to true four-force recordings for all 16 switches.
- **Free libraries:** Freesound.org shows a license on every sound. CC0 is free to use; CC-BY is fine with credit in the About window. Skip CC-BY-NC, because "non-commercial" blocks charging later.
- **Open-source packs:** some community keyboard-sound packs, such as those made for Mechvibes, carry licenses that allow reuse, but many have none. Use only packs with an explicit license allowing commercial use, and keep a copy of it.
- **Paid sound libraries:** royalty-free sound-effect libraries sold for commercial use often include mechanical keyboards. Check that the license covers use inside software.
- **Sources with one force level:** `build_pack.py --derive-tiers` creates the missing tiers from one take: lower gain and a gentle low-pass for soft, a slight high-shelf and more gain for slam. It sounds less real than true recordings, so those tiers are marked derived and replaced later.
- **Paper trail:** each pack folder gets a `SOURCES.md` with the URL, license, author, download date and the license text.
- **Recording spec for commissions:** per key group (alpha, space, enter, backspace, modifier, tab), 6–10 takes at each of the four forces plus 6 release takes; 48 kHz, 24-bit; mic 15–30 cm away in the same spot for every pack; quiet room; 0.5 s of silence between takes.
- **Names:** switch names are their makers' trademarks. Describing a recording with them is common practice, but confirm with a lawyer before launch.
