# Changelog

All notable changes are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow
[Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- Repository scaffold: layered C11 core (`libkfcore`), Objective-C app shell layout, dev tools,
  test harness, build and lint configuration, CI, and architecture docs.
- Core foundations, implemented and tested: lock-free SPSC queues, key map, velocity estimation,
  tier blending, auto-adapt, calibration fitting, voice emission, sensor key resolution.
- `latency-bench` tool and `make check-rt` real-time symbol guard.
- Milestone 0 tools: `hid-dump` (HID inventory and a timed presence probe), `sensor-log`
  (raw reports with key-down/up markers, raw source timestamps per ADR-0010), `analyze.py`
  (layouts, clock units, sample rate, per-keystroke peaks and delays, the gate, noise floors,
  plots, fixtures) and `m0_protocol.py` (guided test protocol). Shared tool code in
  `Tools/common/`, with C and Python unit tests (`make test-py`).
- First Milestone 0 findings in `docs/SENSOR_NOTES.md`: the `accel` device (Bosch BMI286 behind
  the SPU) opens without root or Input Monitoring but sends nothing while idle (ADR-0011).
- Milestone 1, core sound pipeline: CoreAudio output engine (default output unit, per-process
  128-frame buffer request, device and sample-rate change notices), lock-free voice pool with
  oldest-voice stealing, constant-power pan with stereo width, smoothed volume, mute, soft
  clipper, and lock-free pack hand-off; ExtAudioFile pack decoder with the spec's limits and
  fallback rules; the event-tap key capture with modifier, auto-repeat and built-in keyboard
  handling; `KFPackManifest` (`pack.json` validation); `kf-cli` (typing, `--demo`, `--idle`);
  `engine-stress`, `mix-bench`, `audio-info`, `system_snapshot.py`; render benchmark in
  `latency-bench`.
- Milestone 2, force detection: parameterized report decoder (int16, int32 or float32 triples,
  scaled to g), the sensor thread (read-only device open, report callback, pending-key watchdog,
  device removal), live calibration hand-over, measured peaks for the calibration UI, Shout
  Mode's rolling force, noise-floor measurement, `kf-cli --calibrate`. Dormant until the report
  layout is measured (ADR-0011): the probe reports no sensor and keys play at default velocity.
- Milestone 3, the menu bar app (`make app`): status item with on, off and needs-attention
  icons; menu with the on/off switch, volume, packs, force status, live stats while open, Labs,
  calibration, settings, updates, feedback; power state machine (idle costs nothing: audio and
  the sensor run only while typing); sleep, lock and session handling; output device and rate
  changes; mic-in-use mute (per process on macOS 14+); settings window with General, Sound,
  Packs, Labs, Stats and About; onboarding with Input Monitoring and the stats opt-in;
  calibration wizard; launch at login through SMAppService (off by default); local daily stats;
  "Remove All Keyfeel Data"; app-layer tests (`make test-app`).

### Changed
- `make analyze` covers the tools; `make ci` also runs `test-py` and builds the tools.
- `make format` and `make lint` find clang-format and clang-tidy in the Xcode toolchain or
  Homebrew LLVM when they are not on `PATH`.
- Matplotlib's cache stays in `build/mplconfig` instead of `~/.matplotlib`.
