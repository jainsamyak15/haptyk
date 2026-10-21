# Keyfeel

Velocity-sensitive mechanical keyboard sounds for Apple Silicon MacBooks. Keyfeel reads the
MacBook's built-in motion sensor about 1,000 times a second and plays a soft, medium, hard or
slam recording for every keystroke, at near-zero CPU, memory and battery cost.

**Status:** Milestone 0, sensor discovery. The discovery tools are built and tested; the sensor
is found but sends nothing while idle, so the gate waits on the owner's decision in
[ADR-0011](docs/DECISIONS.md). The core's foundations are implemented and tested;
hardware-facing code is stubbed with `TODO(Mx)` markers. See [Milestones](#milestones).

## Requirements

| | |
| --- | --- |
| Mac | Apple Silicon MacBook Air or Pro (the motion sensor exists only there) |
| macOS | 13 Ventura or later |
| Toolchain | Xcode or the Xcode Command Line Tools |
| Python | 3.10 or later, for dev tools only, in a repo-local virtual environment |
| Optional | `clang-format` and `clang-tidy` for `make format` and `make lint` |

## Quick start

```sh
make bootstrap   # checks the toolchain, creates Tools/py/.venv inside the repo
make test        # core unit tests
make ci          # everything CI runs
```

Nothing in this repository installs software globally, needs `sudo`, or changes system
settings. See the Safety rules in [CLAUDE.md](CLAUDE.md).

## Layout

```
App/                  macOS app shell (Objective-C, AppKit), from M3
  Sources/<module>/   one folder per feature module (see App/README.md)
  Resources/          Info.plist, entitlements, bundled packs, calibration defaults
Config/               xcconfig build settings: shared, per configuration, per target
Core/                 libkfcore, the C11 real-time core
  include/kf/         public API: the only headers the app may include
  src/<module>/       implementation; *_rt.c files hold real-time code only
  src/internal/       header-only primitives: SPSC queue, RNG, selection
Packs/                sound sourcing workflow and licence records
Tests/
  Core/               C unit tests (make test)
  App/                XCTest for the app layer, from M3
  Fixtures/           recorded sensor reports and other test data
Tools/                dev tools, never shipped: C tools and Tools/py
docs/                 spec, architecture, decisions, performance, privacy, sensor notes
scripts/              bootstrap, real-time symbol check, release
```

## Make targets

| Target | What it does |
| --- | --- |
| `make test` | Core unit tests |
| `make test-asan`, `make test-tsan` | Tests under AddressSanitizer + UBSan, and ThreadSanitizer |
| `make bench` | Hot-path latency against the budget |
| `make check-rt` | Fails if real-time code imports a banned symbol |
| `make analyze`, `make lint` | Clang Static Analyzer, clang-tidy |
| `make format`, `make format-check` | clang-format |
| `make tools` | Dev tools into `build/<config>/bin` |
| `make test-py` | Unit tests for the Python dev tools |
| `make test-pack` | Synthesizes the debug "Test Click" pack |
| `make ci` | Everything CI runs |

Configurations: `CONFIG=debug` (default), `release`, `asan`, `tsan`.

## Milestone 0 tools

```sh
make tools bootstrap
build/debug/bin/hid-dump --transport SPU                     # find sensor candidates
build/debug/bin/hid-dump --transport SPU --usage-page 0xff00 --usage 3 --probe
Tools/py/.venv/bin/python Tools/py/m0_protocol.py --surface desk   # guided protocol
Tools/py/.venv/bin/python Tools/py/analyze.py report logs/<machine>/*.csv
```

Findings and the commands still to run are in [docs/SENSOR_NOTES.md](docs/SENSOR_NOTES.md).

## Milestones

| | Milestone | Status |
| --- | --- | --- |
| M0 | Sensor discovery (go/no-go gate) | Tools done; blocked on ADR-0011 (starting the sensor) |
| M1 | Core sound pipeline (command line) | Done: engine, packs, key tap, `kf-cli` |
| M2 | Force detection (command line) | Built and tested; dormant until the sensor layout is measured |
| M3 | Menu bar app | Not started |
| M4 | Sound packs | Not started |
| M5 | Shipping pipeline | Not started |
| M6 | Labs and polish | Not started |

## Documentation

| Document | Contents |
| --- | --- |
| [docs/SPEC.md](docs/SPEC.md) | Product and engineering spec |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Layers, modules, threads, data flow |
| [docs/DECISIONS.md](docs/DECISIONS.md) | Architecture decision records |
| [docs/PERFORMANCE.md](docs/PERFORMANCE.md) | Budgets, how to measure, results log |
| [docs/SENSOR_NOTES.md](docs/SENSOR_NOTES.md) | Milestone 0 findings |
| [docs/PRIVACY.md](docs/PRIVACY.md) | What is and is never collected |
| [docs/RELEASING.md](docs/RELEASING.md) | Signing, notarization, updates |
| [Packs/README.md](Packs/README.md) | Sourcing and building sound packs |
| [CLAUDE.md](CLAUDE.md) | Rules for Claude Code |

## Licence

Proprietary. See [LICENSE](LICENSE).
