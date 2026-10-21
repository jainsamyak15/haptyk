# Architecture

Keyfeel is a small C11 real-time core (`libkfcore`) under a thin Objective-C/AppKit shell. The
core owns everything on the path from keystroke to speaker; the shell only starts, stops and
configures it.

## Layers

```
 App (Objective-C, AppKit)            App/Sources/
   may include only <kf/...>
            │
            ▼
 Core public API                      Core/include/kf/      stable C contract
            │
            ▼
 Core modules                         Core/src/<module>/    own headers, internal/,
            │                                               other modules' public headers
            ▼
 Internal primitives                  Core/src/internal/    header-only, no dependencies
```

Rules:

- Dependencies point down only. No cycles between modules.
- Tests and dev tools may include `Core/src/` headers; the app may not.
- macOS frameworks are used only inside the module that needs them (CoreGraphics in keytap,
  IOKit in sensor and keytap, AudioToolbox in engine and pack), so the rest stays portable and
  host-testable.

## Modules

| Module | Responsibility | Public header | Real-time files | Uses |
| --- | --- | --- | --- | --- |
| common | Status codes, the shared clock | `kf_types.h`, `kf_time.h` | — | — |
| keymap | Key code → group, row, pan | `kf_keymap.h` | — | common |
| keytap | System-wide key capture, routing | `kf_keytap.h` | `kf_keytap_rt.c` | keymap, sensor, engine |
| sensor | Motion sensor, pending-key resolution | `kf_sensor.h`, `kf_accel.h` | `kf_sensor_rt.c` | velocity, engine |
| velocity | Peak, calibration, tiers, auto-adapt | `kf_velocity.h` | `kf_velocity_rt.c`, `kf_autoadapt_rt.c` | internal |
| engine | Voice queues, mixer, CoreAudio output | `kf_engine.h` | `kf_emit_rt.c`, `kf_render_rt.c` | pack, velocity |
| pack | Decoded sound packs in RAM | `kf_pack.h` | — | — |

## Threads

| Thread | Created by | Runs | Priority | May block |
| --- | --- | --- | --- | --- |
| Main | AppKit | UI, power state machine, pack loading, telemetry | Default | Yes, never on the hot path |
| Keytap | `keytap` (pthread + CFRunLoop) | Event-tap callback, routing | User-interactive | No |
| Sensor | `sensor` (pthread + CFRunLoop) | HID callback, ring, key resolution | User-interactive | No |
| Audio | CoreAudio | Render callback, mixing | Real-time | Never |

## Keystroke data flow

```
keyboard
   │
   ▼
[keytap thread]  CGEventTap → kf_keymap_make_event() → kf_keytap_dispatch()
   │
   ├─ built-in key-down, sensor LIVE ──► pending queue (SPSC) ──► [sensor thread]
   │                                                              HID report → decode
   │                                                              → ring buffer
   │                                                              → resolve keys whose window closed
   │                                                              → kf_emit_keystroke(queue B)
   │                                                                        │
   └─ everything else ──► kf_emit_keystroke(queue A) ───────────────────────┤
                                                                            ▼
                                                     [audio thread] kf_engine_render()
                                                     drain A + B → voices → mix → output
```

The sensor thread resolves keys because it is the only thread that knows when a key's window of
samples has arrived. That needs no timers and adds no wakeups.

## Real-time translation units

Code that runs on the audio thread, the sensor callback or the keytap callback lives in files
named `*_rt.c` and is marked `KF_RT_SAFE`. `make check-rt` compiles the release core and fails if
any `*_rt.o` imports a symbol that can allocate, lock, block, log, do I/O or send Objective-C
messages (`scripts/check-rt-symbols.sh`). Keeping real-time code in its own files is what makes
this check possible, so never mix real-time and ordinary code in one file.

## State ownership

| State | Owner | How other threads touch it |
| --- | --- | --- |
| Voice queues A and B | One producer each (keytap, sensor); audio thread consumes | SPSC queue |
| Pending-key queue | Keytap produces; sensor consumes | SPSC queue |
| Sample ring, calibration, auto-adapt | Sensor thread while running; main thread while stopped | Not shared |
| Sensor state, last sample time, counters | Sensor | C11 atomics |
| Volume, mute | Main writes | C11 atomics, read by the audio thread |
| Active pack | Main builds; audio thread uses | Atomic pointer swap + epoch (M1) |

## Errors

Fallible core functions return `kf_status`. Real-time functions never fail loudly: they count
problems in atomic counters (dropped and stale requests, tap timeouts, dropped samples), which
Export Diagnostics reports. Release builds never abort.

## Testing

- `Tests/Core/`: plain-C unit tests (ADR-0004), run plain and under ASan, UBSan and TSan.
- `make bench`: hot-path latency against the budget in `docs/PERFORMANCE.md`.
- `Tests/App/`: XCTest for the Objective-C layer, from M3.
- Hardware behaviour is verified with the Milestone 0 tools and the manual matrix in the spec.
