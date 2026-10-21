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

### Changed
- `make analyze` covers the tools; `make ci` also runs `test-py` and builds the tools.
- `make format` and `make lint` find clang-format and clang-tidy in the Xcode toolchain or
  Homebrew LLVM when they are not on `PATH`.
- Matplotlib's cache stays in `build/mplconfig` instead of `~/.matplotlib`.
