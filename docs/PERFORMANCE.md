# Performance

Budgets are acceptance criteria, measured on an M1 MacBook Air, the slowest supported Mac.
A regression above 10% on any budget blocks a milestone until it is fixed or the owner
approves it.

## Budgets

| Metric | Budget | Measured with |
| --- | --- | --- |
| Hot path: key event → voice queued (no sensor wait) | p99 < 200 µs | `make bench` |
| Sensor wait after key-down (POST window) | ≤ 10 ms, TBD-M0 | Milestone 0 analysis |
| Audio output buffer | 128 frames (2.7 ms at 48 kHz) | Engine config |
| Audio callback CPU time | p99 < 10% of the buffer period | Engine counters, Instruments |
| CPU while idle (no typing for 30 s) | 0.0% | Activity Monitor, `top` |
| Wakeups while idle | < 1 per second | `sudo powermetrics --samplers tasks` (owner runs it) |
| CPU while typing 600 keys/min | < 1% | Activity Monitor |
| Decoded audio for the active pack | ≤ 8 MB | Pack loader |
| Memory footprint | < 40 MB | Activity Monitor |
| DMG size with all bundled packs | < 30 MB | `scripts/release.sh` |

## How to measure

- `make bench`: release build of `latency-bench`; fails when p99 exceeds the budget.
- `make check-rt`: confirms no real-time file imports a banned symbol.
- Instruments: Time Profiler for CPU, Allocations for zero allocations on the audio and sensor
  threads after start-up, System Trace for wakeups.
- `pmset -g assertions` (read-only): nothing from the app remains 30 s after typing stops.
- Before each release: an 8-hour soak with `engine-stress` and real typing; memory stays flat
  and `leaks` reports zero leaks.

## Results log

Newest first. Record the machine, build and numbers for every milestone.

| Date | Milestone | Machine | Hot path p50 / p99 / max | Notes |
| --- | --- | --- | --- | --- |
| 2026-10-23 | M4 packs | MacBook Air M5 (Mac17,3), macOS 27.0 | core unchanged | 23 bundled packs; local DMG 20.7 MB (limit 30 MB). Earlier, with 19 packs: 18.1 MB on disk (CAF/ALAC), release app 21 MB, DMG 17.4 MB. Largest pack decodes to 1.8 MB (limit 8 MB), smallest 0.2 MB. `Keyfeel.app` (debug) idle with the default pack loaded: 0.0% CPU, 18 MB (budget 40 MB), 4 threads, no network socket. Launch left no change on the Mac but the app's own folder (snapshots 11 and 12). |
| 2026-10-22 | M6 | MacBook Air M5 (Mac17,3), macOS 27.0, Apple clang 21, `-O3` | 0.5 / 0.96 / 26.8 µs with Big Enter on, 0 drops | No regression from M1 (0.5 / 0.96). Render, 24 voices x 128 frames: p50 0.83 / p99 0.92 µs. Mixer C 424 ns vs vDSP 929 ns. Shout Mode's decision in the active tap: p50 41 / p99 42 ns (the timer's resolution; budget 50 µs). `Keyfeel.app` (debug, 1.4 MB), Labs at their defaults, idle with no window: 0.0% CPU in every `top` sample over about 8 minutes, 2 idle wakeups in that time, 16 MB, 3-5 threads, no network socket. Force Meter: a 50 ms timer with 10 ms leeway, only while typing with the feature on; none while idle. Big Enter's boom: 84 KB, in memory only while on and force is measured. Launch left no change on the Mac but the app's own folder (snapshots 09 and 10). |
| 2026-10-22 | M5 | MacBook Air M5 (Mac17,3), macOS 27.0 | core unchanged from M1 | `Keyfeel.app` (debug, 1.3 MB) idle with no window: 0.0% CPU, 15 MB, 3-4 threads, 1 wakeup in 10 s, no network socket while `Info.plist` holds placeholder URLs. Launch left no change on the Mac but the app's own folder (snapshots 07 and 08). |
| 2026-10-22 | M3 | MacBook Air M5 (Mac17,3), macOS 27.0 | core unchanged from M1 | `Keyfeel.app` (debug), idle with no window: 0.0% CPU in all 10 samples, 1 wakeup in 20 s, 16 MB footprint, 3 threads. With onboarding open (permission poll once a second, as the spec says): 23 MB, about 1 wakeup a second. Typing 600 keys a minute through the same pipeline (`kf-cli`, release): 0.2-0.4% CPU, 5.9 MB, callback p99 4 us, 0 overruns. App bundle 1.2 MB. Launch left no change on the Mac but the app's own folder (system snapshots). |
| 2026-10-21 | M1 | MacBook Air M5 (Mac17,3), macOS 27.0, Apple clang 21, `-O3` | 0.5 / 0.96 / 29.6 µs, 0 drops | Render, 24 voices x 128 frames: p50 0.83 / p99 0.92 / max 11.3 µs (budget 267 µs). Mixer C 424 ns vs vDSP 932 ns (ADR-0013). `kf-cli` (debug): 128-frame buffer granted to this process only (other processes still see 512), callback p99 8 µs, start to first callback 30-34 ms, 0 overruns; idle with the engine stopped 0.0% CPU and no wakeups over 14 s (`top`), footprint 5.6 MB. `engine-stress` 2 min, 30 voices/s, 12 pack swaps: 0 overruns, p99 4 µs, max 10.5 µs, slowest swap 12.5 ms, footprint flat at 7.6 MB (-16 KB). Pack decode: Test Click (90 files) 515 KB in 21-25 ms. System snapshots identical before and after every audio run. |
| 2026-10-21 | M0 | MacBook Air M5 (Mac17,3), macOS 27.0, Apple clang 21, `-O3` | 0.5 / 1.0 / 57.7 µs, 0 drops | Not the budget machine (M1 Air). Timer resolution 41.7 ns (24 MHz). Sensor presence check: p50 22 µs, p99 54 µs. |
| 2026-10-21 | Scaffold | Linux x86-64 sandbox VM, clang 18, `-O3` | 0.5 / 0.8 / 59–94 µs, 0 drops | 1-core VM: max is scheduler jitter. Not a Mac; re-measure on the M1 Air in M1. |
