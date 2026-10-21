# App

The macOS menu bar app (Objective-C, ARC, AppKit). Built from M3; spec section "App shell".
It includes only `<kf/...>` headers and talks to the core through that API.

| Folder | Classes | Responsibility | Milestone |
| --- | --- | --- | --- |
| `Application/` | `main.m`, `KFAppDelegate` | Launch, owns the controllers | M3 |
| `Power/` | `KFPowerController` | Idle/active state machine, sleep/wake, device changes, mic-in-use mute | M3 |
| `Menu/` | `KFStatusMenuController`, `KFVolumeMenuView` | Status item, menu, live stats while open | M3 |
| `Onboarding/` | `KFOnboardingWindowController` | Permissions, stats opt-in, first sound test | M3 |
| `Settings/` | `KFSettingsWindowController` | Tabbed settings, created on demand, released on close | M3 |
| `Calibration/` | `KFCalibrationController` | Calibration wizard (math in `kf_calibration_compute`) | M3 |
| `Packs/` | `KFPackLibrary` | Parses `pack.json`, fallbacks, import (ditto), delete | M4 |
| `Stats/` | `KFStatsController`, `KFBadgeCardRenderer` | Local stats, percentile, badge, share card | M5 |
| `Telemetry/` | `KFTelemetry`, `KFDiagnostics` | Opt-in daily upload, MetricKit crash reports, Export Diagnostics | M5 |
| `Labs/` | `KFLabsController` | Big Enter, Shout Mode, Force Meter | M6 |
| `Preferences/` | `KFPreferences` | Typed wrapper around `NSUserDefaults` | M3 |
| `Support/` | Shared helpers | Logging (`os_log`, never key data), constants | M3 |

Rules: windows are created when opened and released when closed; no timers while idle; nothing
here runs on a real-time thread.
