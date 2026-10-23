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

- **Status:** Superseded by ADR-0015
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

- **Status:** Accepted (approved with M1–M6 on 2026-10-21; refines the spec's
  `t_ns,report_hex` rows)
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
- **Decision:** Option 2 is not pursued: no tool or app code writes the sensor driver's
  properties. The app reads the sensor only through a plain open of the HID device. Until that
  delivers samples, Keyfeel runs in fallback mode: every key plays at the default velocity.
  Option 1 (the read-only root test) remains the owner's call.
- **Consequences:** M1–M6 are built and tested in fallback mode. The sensor path is
  implemented against the documented interface and stays dormant (`KF_SENSOR_ABSENT`) while
  the device sends nothing.

## ADR-0012: Pack fallback rules are applied in the core

- **Status:** Accepted (refines the spec: "KFPackLibrary ... applies the fallback rules")
- **Context:** If the Objective-C layer resolved fallbacks by listing files again, every
  fallback tier or group would be decoded a second time, using up to 6x the memory the pack
  needs.
- **Decision:** `KFPackManifest` validates `pack.json` and passes the files as listed.
  `kf_pack_build()` applies the rules (nearest tier, softer on ties; missing group uses alpha;
  missing release means none) by pointing slots at already-decoded audio.
- **Consequences:** No duplicated audio; the rules are unit-tested in C on any host.

## ADR-0013: The mixer is auto-vectorized C, not vDSP

- **Status:** Accepted
- **Context:** The spec says to benchmark auto-vectorized C loops against Accelerate (vDSP) and
  keep the faster.
- **Decision:** Keep the C loops. `make bench` (`mix-bench`, MacBook Air M5, -O3): 24 voices of
  128 frames take 424 ns in C and 932 ns with `vDSP_vflt16` + two `vDSP_vsma`. The compiler
  fuses conversion and both channels into one pass, while vDSP makes three passes and pays a
  call per pass.
- **Consequences:** No Accelerate dependency in the core. `mix-bench` stays in `make bench` so
  a future SDK or chip that changes the answer shows up.

## ADR-0014: Key events are stamped on arrival

- **Status:** Accepted, until Milestone 0 measures `CGEventGetTimestamp()`
- **Context:** The spec stamps keys with the event's own time, converted to nanoseconds, but its
  units are one of the open Milestone 0 measurements.
- **Decision:** The keytap stamps each event with `kf_time_now_ns()` when its callback runs. HID
  keyboard reports, used to tell built-in from external keys, are stamped the same way on the
  same thread, so the two stay comparable.
- **Consequences:** Times are late by the tap's delivery delay (measured with `sensor-log`
  once available). That only matters for force detection's window, which is inactive while the
  sensor delivers no data (ADR-0011).

## ADR-0015: The app is built by the Makefile, not Xcode

- **Status:** Accepted (supersedes ADR-0003; the owner approved M1–M6 on this Mac)
- **Context:** The build Mac has the Command Line Tools but no Xcode, so there is no
  `xcodebuild`, XcodeGen or XCTest. The spec asks for one Xcode project.
- **Decision:** `make app` compiles the Objective-C (ARC, `-Os` in release) and the app's C,
  links `libkfcore.a`, generates `Info.plist` from `Config/App.xcconfig`, copies resources and
  signs: ad hoc for local builds, Developer ID in `scripts/release.sh`. App-layer tests use the
  same dependency-free harness approach as ADR-0004 (`Tests/App`, `make test-app`).
  `project.yml` stays for an owner who installs Xcode later.
- **Consequences:** One build, scriptable and reviewable, with no project file to merge. Ad-hoc
  signatures change on every build, so Input Monitoring must be granted again after rebuilding
  a debug app.

## ADR-0016: A third voice queue for the main thread

- **Status:** Accepted
- **Context:** Previews (Packs pane, onboarding) play sounds from the main thread. Pushing them on
  the keytap's or the sensor's queue would give that SPSC queue two producers.
- **Decision:** `KF_QUEUE_MAIN`, produced only by the main thread; the engine drains all queues.
- **Consequences:** Every queue keeps exactly one producer, as the real-time rules require.


## ADR-0017: Big Enter's boom travels with the pack

- **Status:** Accepted
- **Context:** The spec plays the Labs boom "as an extra voice request from the sensor thread"
  on the same engine. The boom must be decoded at the device rate, re-decoded when the rate
  changes, and never freed while a voice plays it: exactly what the pack hand-off already does.
- **Decision:** A pack's index has one more row, `KF_SOUND_BOOM`, after the key groups. While Big
  Enter is on and force detection exists, `KFPackLibrary` decodes `Resources/Labs/boom.wav` into
  the pack (one file more); `kf_emit_keystroke()` adds a boom request when an Enter key-down's
  velocity is at least `KF_BIG_ENTER_VELOCITY` and `kf_engine_set_big_enter()` is on. No
  fallback rule reads or fills that row. Turning the feature on or off reloads the pack in the
  background, through the usual swap. A boom that fails to decode is left out and the key
  sounds still load. The boom itself is synthesized by `Tools/py/make_labs_sounds.py`, so the
  project owns it; a test keeps the committed file equal to the script's output.
- **Consequences:** One ownership and swap path for all audio, no second pending pointer and no
  new lifetime rules. The 84 KB boom is in memory only while it can play. Only a measured
  velocity reaches the threshold, so in fallback mode Big Enter is inert and costs nothing.

## ADR-0018: Shout Mode runs an active tap only when it can act, and steps aside quietly

- **Status:** Accepted
- **Context:** An active (filtering) event tap that is slow delays every keystroke on the Mac,
  and it needs Accessibility. Shout Mode can only act when force is measured. The spec asks to
  skip Secure Input and to "tell the user" when repeated tap timeouts turn the feature off. The
  public `IsSecureEventInputEnabled()` is no longer declared in the macOS 26 SDK.
- **Decision:**
  - The tap runs in active mode only while Shout Mode is on, force detection exists
    (`kf_sensor_probe()`) and Accessibility is granted. Otherwise it stays listen-only, and
    Accessibility is not even asked for. If macOS refuses the active tap, a listen-only tap
    starts at once, so sounds never stop.
  - The callback's decision is pure (`kf_keytap_shout_flags()`): Shift is added only to the 26
    letter key codes, never under Command, Control or Option, only for built-in keys while the
    sensor is live. It reads no characters. Secure Input is enforced by macOS itself, which
    delivers no key events to event taps while it is on; a manual test row checks it.
  - The core counts tap timeouts while Shout Mode is on and turns it off itself at the third
    within a minute, then raises a flag. The app's key callback, which runs anyway, notices it
    with one atomic load and hops to the main thread once. The app reverts to a listen-only tap
    and shows the attention dot and a menu line until the user opens the menu: no modal alert
    that would steal keystrokes mid-sentence, and no notification permission prompt.
- **Consequences:** On every Mac without measured force (today, all of them: ADR-0011) Shout Mode
  never filters a keystroke. The decision costs about 40 ns per key-down (`make bench`).

## ADR-0019: Bundled packs from licensed Freesound typing recordings

- **Status:** Accepted, amended by ADR-0020: five Freesound packs remain (the owner's approval of
  each pack's sound is still required, spec M4)
- **Context:** The spec's 16 packs need recordings licensed for commercial use in an app, and
  says to rename or drop any that can't be sourced legally. Commissioned four-force sessions
  are not available. Freesound holds CC0 and CC BY recordings of real keyboards, but as
  continuous typing. The apps and community packs that use the spec's switch names carry no
  recording credits or licence.
- **Decision:** Ship the eleven packs that licensed recordings support (ten CC0 1.0, one CC BY
  4.0 credited in About), each named after what was recorded. Cut keystrokes out of typing with
  `extract_keystrokes.py`: a press and its release, isolated by quiet; their press/release
  spectra separate in a leave-one-out test (about 0.6 to 0.9; 0.5 would mean two presses) and
  hold times run 90 to 110 ms. Tiers come from loudness quartiles of the typist's natural
  variation, or `--derive-tiers` when a recording has fewer than 16 clean keystrokes.
  `Packs/packs.json` plus `build_packs.py` make every pack reproducible, and an original file
  replaces the public preview when the owner downloads it. The packs (5.3 MB of CAF/ALAC) are
  committed as ordinary binary files: ADR-0007 (Git LFS) is still only proposed.
- **Consequences:** v1 ships with real recordings and a complete paper trail, but only alpha
  sounds (other keys fall back to them), tiers that are not force-controlled, and lossy-preview
  sources until the originals are downloaded. Alpaca, Biscuit, EG Crystal Purple, Gateron Black
  Ink, Gateron Red Ink V2, Kailh Box Navy, NK Cream and Turquoise Tealios wait for commissioned
  recordings (Packs/README.md). Pack names are switch makers' trademarks used descriptively:
  confirm with a lawyer before launch (Appendix B).

## ADR-0020: Mechvibes and kbsim packs under their MIT licences

- **Status:** Accepted (owner's decision, 2026-10-23)
- **Context:** Eight of the spec's sixteen switches have no Freesound recording licensed for
  commercial use (ADR-0019). Mechvibes (© 2021 Hai Nguyen) and kbsim (© Thomas Lai) publish
  packs for them in repositories under the MIT License, which permits commercial use, copying
  and redistribution, provided the copyright and permission notice travel with the copies. The
  spec's Appendix B said never to copy audio from another app; the owner has chosen to rely on
  these licences instead.
- **Decision:** Take Cherry MX Black, Blue, Brown and Red PBT, EG Crystal Purple, Topre PBT and
  NK Cream from Mechvibes, and Alpaca, Blue Alps, Gateron Black Ink, Gateron Red Ink, Holy Panda,
  Kailh Box Navy and Turquoise Tealios from kbsim, fetched at pinned commits by
  `build_packs.py`. Each pack's `SOURCES.md` holds the repository's MIT notice, and About shows
  an attribution line for each. The Freesound recordings of the same switches are dropped, so
  each name appears once; Buckling Spring stays on Freesound's CC0 recording, and four
  Freesound-only switches stay as extras. Biscuit is not available anywhere and is left out.
- **Consequences:** Fifteen of the sixteen spec packs ship, with per-key-group sounds. The
  repositories credit no recordist (NK Cream names "Ryan"), so the licence rests on the
  repositories' authors having had the right to grant it; if a recordist objects, that pack
  must be replaced, ideally by a commissioned recording. Packs total 18 MB (DMG budget 30 MB);
  the largest decodes to 1.8 MB (budget 8 MB).

## ADR-0021: Pack quality survey; ABS variants added, clipped samples and extra takes trimmed

- **Status:** Accepted
- **Context:** The owner asked for the best free sound packs, licensed or not. Every free source
  was measured for bandwidth (where lossy coding cut the top end), noise and clipping: Mechvibes'
  default and 94 community packs, kbsim, seven open-source apps that bundle them, Freesound and
  OpenGameArt.
- **Decision:**
  - Add Mechvibes' four Cherry MX ABS-keycap packs (MIT; 17-22 kHz, 79-85 dB SNR), the best
    free recordings found.
  - Keep the kbsim packs: no free recording of those switches measures better (community copies
    are the same files; Tealios V2 by Akira measures lower).
  - Do not bundle community packs without a licence, however good (Razer Green by Akira, Kailh
    Box White): ask their authors for written permission first. Do not use GPL or AGPL audio.
  - Skip any Mechvibes sample with real clipping (three or more samples in a row at full scale);
    single overs from Ogg decoding are kept.
  - Keep at most 4 takes for each non-alpha group (10 for alpha), since modifiers are pressed far
    less: the 23 packs fit in a 20.7 MB DMG (limit 30 MB).
  - Where a switch has several licensed recordings, keep the cleanest, not the most permissive:
    Buckling Spring moves to fastson's CC BY 3.0 PCM-M10 recording (43.5 dB keystroke SNR,
    24 kHz) from a CC0 lavalier one (33.5 dB, 20.4 kHz), credited in About.
- **Consequences:** The weakest packs remain the seven from kbsim (lossy MP3, 13.6-18 kHz). Their
  quality can only rise through permission from community authors or new recordings.

## ADR-0022: Community packs with their author's permission

- **Status:** Accepted (owner's decision, 2026-10-23)
- **Context:** Kailh Box White, a high-quality Mechvibes community pack (ADR-0021), carries no
  licence. The owner asked its author, edy., who said the pack is open for anyone to use. The
  owner also reports the same for Razer Green by Akira, but that pack's zip ships a GNU GPL v3
  licence file, which mechvibes.com does not show.
- **Decision:** A new pack kind, `mechvibes-community`: the published zip, pinned by SHA-256, read
  like a Mechvibes pack (missing files and clipped sounds skipped), with the permission recorded in
  `SOURCES.md`, the author credited in About, and the author's written reply kept in
  `Packs/sources/<id>/permission/`. Kailh Box White is bundled this way. Razer Green is bundled
  too, on the owner's instruction, under the GPL v3 it ships with: its `SOURCES.md` holds the
  licence text and lists the changes, the original files it was made from (with `config.json`
  and `LICENSE.txt`) travel inside the pack in `original/`, and the pack stays a separate data
  file the app loads, not part of the app's code.
- **Consequences:** Each permission rests on the author's reply, which must be saved. Razer
  Green's sounds stay under the GPL v3 for anyone who receives them; treating a data file as
  separate from a closed-source program is common practice but not tested in court, so a
  written grant from Akira for use without the GPL terms would remove the remaining risk.

## ADR-0023: A status panel in the "Graphite & Signal" theme; the menu stays on right-click

- **Status:** Accepted (owner's request, 2026-10-23)
- **Context:** The spec's status item opens an NSMenu. The owner asked for a better interface
  than competitors' panels, in a current colour theme with no gradients, purple or pink.
- **Decision:**
  - A left click opens a borderless, non-activating panel under the status item
    (`KFStatusPanelController`): header with the power switch, an attention banner only when
    needed, a Feel card (the last keystroke's tier as four rising bars and one bar per second of
    the last minute's typing, which works without force detection), volume, the pack chooser
    with switch-type filters, three stat tiles, the Labs toggles and the menu's commands. It
    never steals focus from the app being typed in, closes on Escape or any click elsewhere, and
    refreshes every 0.5 s only while open. A right click or Control-click still opens the
    spec's menu, unchanged.
  - The theme (`KFTheme`): warm graphite neutrals (dark #121211 / light #F5F3EF) with one
    signal-orange accent (#FF6B1F dark, #EC560C light), the colour of an enthusiast keyboard's
    Escape keycap; muted, non-purple tags per switch type; flat fills only; Increase Contrast
    variants; text on the accent in near-black for contrast. Every control is a native control
    underneath, so keyboard use, focus rings and VoiceOver work as standard.
- **Consequences:** The panel is built on first open and kept (a few hundred KB); nothing runs
  while it is closed. `-KFRenderPanel DIR` (debug builds) draws it in light and dark for design
  review without screen capture.

## ADR-0024: A "Glass" panel style, like Control Center

- **Status:** Accepted (owner's request, 2026-10-23)
- **Context:** The owner wants users to be able to choose an Apple-style glass look (macOS
  Control Center) instead of Graphite (ADR-0023).
- **Decision:** A `panelStyle` preference ("graphite", the default, or "glass") in Settings >
  General. In Glass the panel has no background: each section is its own translucent tile over
  the desktop, grouped in an `NSGlassEffectContainerView`. On macOS 26 and later each tile's
  background is a real `NSGlassEffectView` (Liquid Glass) with a light tint for legibility; the
  tile's content sits above it, since Liquid Glass sizes its own content view to a preferred
  height. Before macOS 26 the tiles use the system blur (`NSVisualEffectView`, popover material)
  with rounded masks and a bright edge. Active controls turn white (near-black in light
  appearance), as Apple's do; signal orange stays only in the wordmark. Corner radii grow (x1.6).
  All colours stay theme tokens, so a style change is a rebuild at the next open.
- **Consequences:** Two layouts share one set of controls and tests. Previews (`-KFRenderPanel`)
  draw the glass from a blurred copy of the desktop picture, since an offscreen drawing cannot
  see behind a window; the same run builds the live glass offscreen and checks every tile's size.
