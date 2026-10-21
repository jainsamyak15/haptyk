# Architecture decision records

Each record states the context, the decision and its consequences. Status is **Proposed**
(needs the owner's approval), **Accepted** or **Superseded**. Never edit an accepted record's
decision; supersede it with a new one.

## ADR-0001: C11 real-time core, Objective-C/AppKit shell

- **Status:** Accepted
- **Context:** The product's value is the lowest possible keystroke-to-sound latency at
  negligible CPU and memory cost, on macOS only.
- **Decision:** Real-time code is C11 with no runtime. The app shell is Objective-C with ARC on
  AppKit. No Swift, SwiftUI, Electron, Tauri or web views.
- **Consequences:** Smallest footprint and full control over the real-time path. More manual
  UI code than SwiftUI would need.

## ADR-0002: No third-party audio libraries

- **Status:** Accepted
- **Context:** JUCE, PortAudio and miniaudio are cross-platform layers over CoreAudio.
- **Decision:** Use AudioToolbox directly (default output unit, C render callback).
- **Consequences:** Less code to ship and audit; device-change handling is ours to write.

## ADR-0003: Make for the core, Xcode project generated for the app

- **Status:** Proposed
- **Context:** M0–M2 are command-line tools; the app starts at M3. A hand-edited `.pbxproj` is
  hard to review and merges badly.
- **Decision:** The `Makefile` builds the core, tools and core tests from M0. From M3 the app's
  Xcode project is generated from `project.yml` with XcodeGen, and the `.xcodeproj` is not
  committed. Build settings live in `Config/*.xcconfig`.
- **Consequences:** Needs XcodeGen on the owner's Mac (the owner installs it; Claude Code must
  not). If the owner declines, create the project once in Xcode at M3 using the same xcconfig
  files, commit it, and mark this record Superseded.

## ADR-0004: Core unit tests in plain C

- **Status:** Accepted (refines the spec's "Tests: XCTest" row)
- **Context:** Core tests must run fast, from the terminal, under every sanitizer, and in CI
  without an Xcode project.
- **Decision:** `Tests/Core/` uses a dependency-free C harness (`kf_test.h`) run by `make test`.
  XCTest covers the Objective-C layer from M3.
- **Consequences:** Two test runners; each is the natural one for its layer.

## ADR-0005: Real-time code isolated in `*_rt.c`, enforced by symbol check

- **Status:** Accepted
- **Context:** One allocation or lock on the audio thread causes audible glitches, and code
  review alone misses them.
- **Decision:** Real-time code lives only in `*_rt.c` files. `make check-rt` fails if their
  objects import allocation, locking, blocking, logging, I/O or Objective-C runtime symbols.
- **Consequences:** Slightly more files; a mechanical guarantee instead of a convention.

## ADR-0006: JSON is parsed only in the Objective-C layer

- **Status:** Accepted
- **Context:** Packs carry a `pack.json` manifest.
- **Decision:** `KFPackLibrary` parses and validates it with `NSJSONSerialization` and passes
  `kf_pack_load()` a flat file list. The core never parses JSON.
- **Consequences:** No JSON library in the core.

## ADR-0007: Git LFS for bundled pack audio

- **Status:** Proposed
- **Context:** Sixteen bundled packs are tens of MB of binary audio.
- **Decision:** Store `App/Resources/Packs/**/*.caf` in Git LFS (lines prepared, commented out,
  in `.gitattributes`). Raw sources stay out of git; only `SOURCES.md` and licence texts are
  committed.
- **Consequences:** Needs git-lfs, installed by the owner.

## ADR-0008: Portable core, host-testable off the Mac

- **Status:** Accepted
- **Context:** Most core logic is plain math and data structures.
- **Decision:** Only the clock (`kf_time.c`) and the framework-backed parts of keytap, sensor,
  engine and pack use `__APPLE__`. Key codes are mirrored in `kf_vk.h` and checked against the
  SDK at compile time on macOS.
- **Consequences:** CI can run the core tests on Linux too; the product itself stays macOS-only.

## ADR-0009: Status codes for fallible core functions

- **Status:** Accepted
- **Context:** `bool` results lose the reason for a failure.
- **Decision:** Fallible core functions return `kf_status`; real-time functions count failures
  in atomic counters instead.
- **Consequences:** Clear errors for the shell and for Export Diagnostics.

## ADR-0010: sensor-log records raw source timestamps beside arrival times

- **Status:** Proposed (refines the spec's `t_ns,report_hex` rows; implemented in M0 pending
  approval)
- **Context:** Milestone 0 must measure the units of HID report timestamps and of
  `CGEventGetTimestamp()`. Rows that already hold converted nanoseconds can't measure them:
  the conversion would assume the answer.
- **Decision:** Each data row is `t_ns,event,src_ts,data`. `t_ns` is the arrival time on the
  core's clock (`kf_time_now_ns()`, read first in the callback). `src_ts` is the source's own
  timestamp, unconverted. `event` is `REPORT`, `KEYDOWN` or `KEYUP`. `data` is the report in
  hex, empty for keys. `#` lines carry metadata (machine, device, permissions, label) and a
  closing summary. `analyze.py` fits `src_ts` against `t_ns` to measure the units, then uses
  the source times when they share the clock.
- **Consequences:** Two extra columns per row (about 25 bytes). Key rows still hold no key
  code or character. The format is versioned (`format=1`), and `analyze.py` rejects any other
  version.

## ADR-0011: How the app starts the motion sensor's stream

- **Status:** Proposed (needs the owner's decision; blocks the M0 gate)
- **Context:** On a MacBook Air M5 (Mac17,3, macOS 27.0) the `accel` HID device opens for a
  normal user without Input Monitoring, but sends no reports: its driver's `ReportInterval` is 0
  and it has counted no events since boot (`docs/SENSOR_NOTES.md`). Something has to switch
  the stream on, and the only routes known so far change the driver's state. The rules forbid
  that, both while building and in the shipped app, without the owner's approval.
- **Options:**
  1. Root alone starts the stream (test A in the notes). Read-only, but the app would need a
     privileged helper, which is out of scope unless the owner approves it.
  2. Write the driver's reporting properties (test B in the notes), restoring them when done. If
     that needs root, it is option 1 plus a state change. If it works as a normal user, the app
     changes a shared hardware driver's state on customers' Macs.
  3. Neither works: no-go, and the product pivots to sounds without force detection
     (the spec's fallback).
- **Decision:** Pending. Claude Code runs neither test without approval.
- **Consequences:** Until decided, M0 can record only discovery and permission results.
