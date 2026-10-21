#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Proprietary
# Copyright (c) 2026 The Keyfeel Authors. All rights reserved.
"""Read-only snapshot of the Mac state Keyfeel must never change, and a diff of two snapshots.

  system_snapshot.py take DIR      write a snapshot into DIR (must not exist)
  system_snapshot.py diff A B      compare two snapshots; exit 1 when stable state differs

Captured, read-only: motion-sensor drivers (SPU HID devices and drivers in the IORegistry), HID
services, every audio device (default devices, sample rates, buffer sizes, volume, mute),
power settings and assertions, keyboard preferences, launch agents, daemons and privileged
helpers, and the app's own data folders. Each item is saved raw, plus a "stable" view without
counters and timestamps that change on their own, which is what `diff` compares.

Not captured: NVRAM and the permission (TCC) database. Reading them needs commands this
repository's rules block. Standard library only, so it runs without Tools/py/.venv.
"""
from __future__ import annotations

import datetime as _dt
import difflib
import json
import os
import plistlib
import re
import subprocess
import sys
from pathlib import Path
from typing import Any, Callable

REPO = Path(__file__).resolve().parents[2]
AUDIO_INFO = REPO / "build" / "debug" / "bin" / "audio-info"
BUNDLE_ID = "com.example.keyfeel"  # docs/SPEC.md, "Open decisions": default until decided.
HOME = Path.home()

# IORegistry keys that change by themselves (counters, timing, reference counts).
VOLATILE_KEYS = {
    "DebugState",
    "IOPowerManagement",
    "IOGeneralInterest",
    "IOReportLegend",
    "IOReportLegendPublic",
    "timesync",
    "IOUserClientCreator",
    "IORegistryEntryID",
    "IOCFPlugInTypes",
    "CurrentLux",  # Ambient light: changes with the room.
}
KEYBOARD_PREFS = [
    ("-g", "KeyRepeat"),
    ("-g", "InitialKeyRepeat"),
    ("-g", "ApplePressAndHoldEnabled"),
    ("-g", "AppleKeyboardUIMode"),
    ("-g", "com.apple.keyboard.fnState"),
    ("-g", "com.apple.swipescrolldirection"),
]
FOLDERS = [
    HOME / "Library" / "LaunchAgents",
    Path("/Library/LaunchAgents"),
    Path("/Library/LaunchDaemons"),
    Path("/Library/PrivilegedHelperTools"),
]
APP_PATHS = [
    HOME / "Library" / "Application Support" / "Keyfeel",
    HOME / "Library" / "Caches" / BUNDLE_ID,
    HOME / "Library" / "Preferences" / f"{BUNDLE_ID}.plist",
    HOME / "Library" / "HTTPStorages" / BUNDLE_ID,
]


def run(cmd: list[str]) -> tuple[int, str]:
    try:
        done = subprocess.run(cmd, capture_output=True, text=True, timeout=60, check=False)
    except (OSError, subprocess.TimeoutExpired) as exc:
        return 127, f"<{exc}>\n"
    return done.returncode, done.stdout + (done.stderr if done.returncode else "")


def plain(value: Any) -> Any:
    """IORegistry plist values as JSON: bytes become hex, keys sorted, volatile keys dropped.
    Keys starting with "_" are drivers' own statistics, which tick on their own."""
    if isinstance(value, dict):
        return {
            k: plain(v)
            for k, v in sorted(value.items())
            if k not in VOLATILE_KEYS and not k.startswith("_")
        }
    if isinstance(value, list):
        return [plain(v) for v in value]
    if isinstance(value, bytes):
        return value.hex()
    if isinstance(value, (_dt.datetime, _dt.date)):
        return value.isoformat()
    return value


def registry(class_name: str) -> tuple[str, Any]:
    """Every service of `class_name` with its children, as raw XML and stable JSON."""
    code, xml = run(["ioreg", "-a", "-l", "-r", "-c", class_name])
    if code != 0 or not xml.strip():
        return xml, []
    try:
        entries = plistlib.loads(xml.encode())
    except plistlib.InvalidFileException:
        return xml, "<unparsable>"
    return xml, plain(entries)


def capture_meta() -> dict[str, Any]:
    def sysctl(name: str) -> str:
        return run(["sysctl", "-n", name])[1].strip()

    return {
        "taken_utc": _dt.datetime.now(_dt.timezone.utc).isoformat(timespec="seconds"),
        "hw_model": sysctl("hw.model"),
        "os": f"{sysctl('kern.osproductversion')} ({sysctl('kern.osversion')})",
        "boot_time": sysctl("kern.boottime"),
        "uid": os.getuid(),
    }


def capture_audio() -> tuple[str, Any]:
    if not AUDIO_INFO.exists():
        return "audio-info not built: run `make tools`\n", None
    code, text = run([str(AUDIO_INFO)])
    if code != 0:
        return text, None
    data = json.loads(text)
    for device in data.get("devices", []):
        device.pop("running_somewhere", None)  # Other apps start and stop audio constantly.
    return text, data


def capture_keyboard() -> dict[str, str]:
    prefs = {}
    for domain, key in KEYBOARD_PREFS:
        code, out = run(["defaults", "read", domain, key])
        prefs[f"{domain} {key}"] = out.strip() if code == 0 else "<unset>"
    return prefs


def capture_folders() -> dict[str, Any]:
    listing: dict[str, Any] = {}
    for folder in FOLDERS:
        try:
            listing[str(folder)] = sorted(p.name for p in folder.iterdir())
        except FileNotFoundError:
            listing[str(folder)] = "<absent>"
        except PermissionError:
            listing[str(folder)] = "<unreadable>"
    for path in APP_PATHS:
        listing[str(path)] = "present" if path.exists() else "<absent>"
    return listing


def capture_power() -> tuple[str, str, list[str]]:
    settings = run(["pmset", "-g"])[1]
    assertions = run(["pmset", "-g", "assertions"])[1]
    ours = [
        line.strip()
        for line in assertions.splitlines()
        if any(name in line for name in ("Keyfeel", "keyfeel", "kf-cli", "sensor-log"))
    ]
    stable_settings = normalize_power(settings)
    return settings + "\n--- assertions ---\n" + assertions, stable_settings, ours


def normalize_power(text: str) -> str:
    """pmset -g without what other apps hold: "(sleep prevented by ...)" lists their
    assertions, and Keyfeel's own are checked separately."""
    return "\n".join(
        re.sub(r"\s*\((?:sleep|display sleep) prevented by [^)]*\)", "", line)
        for line in text.splitlines()
        if not line.lower().startswith("currently in use")
    )


def stable_lines(path: Path) -> list[str]:
    """A stable file re-normalized with today's rules, so older snapshots stay comparable."""
    if not path.exists():
        return []
    text = path.read_text()
    if path.suffix == ".json":
        text = json.dumps(plain(json.loads(text)), indent=2, sort_keys=True)
    elif path.name == "power-settings.txt":
        text = normalize_power(text)
    return text.splitlines()


def write(path: Path, content: Any) -> None:
    text = content if isinstance(content, str) else json.dumps(content, indent=2, sort_keys=True)
    path.write_text(text if text.endswith("\n") else text + "\n", encoding="utf-8")


def take(directory: Path) -> int:
    if directory.exists():
        print(f"system_snapshot: {directory} exists; choose a new name", file=sys.stderr)
        return 2
    raw, stable = directory / "raw", directory / "stable"
    raw.mkdir(parents=True)
    stable.mkdir()
    write(directory / "meta.json", capture_meta())

    for class_name in ("AppleSPUHIDDevice", "AppleSPUHIDDriver"):
        xml, data = registry(class_name)
        write(raw / f"ioreg-{class_name}.xml", xml)
        write(stable / f"ioreg-{class_name}.json", data)
    write(raw / "hidutil-list.txt", run(["hidutil", "list"])[1])
    audio_raw, audio = capture_audio()
    write(raw / "audio.json", audio_raw)
    write(stable / "audio.json", audio)
    power_raw, power_stable, ours = capture_power()
    write(raw / "power.txt", power_raw)
    write(stable / "power-settings.txt", power_stable)
    write(stable / "power-assertions-keyfeel.json", ours)
    write(stable / "keyboard-prefs.json", capture_keyboard())
    write(stable / "folders.json", capture_folders())
    print(f"snapshot written to {directory}")
    return 0


def diff(a: Path, b: Path) -> int:
    meta_a = json.loads((a / "meta.json").read_text())
    meta_b = json.loads((b / "meta.json").read_text())
    print(f"A: {a} ({meta_a['taken_utc']})\nB: {b} ({meta_b['taken_utc']})")
    if meta_a["boot_time"] != meta_b["boot_time"]:
        print("note: the Mac rebooted between the snapshots")
    changed = 0
    names = sorted({p.name for p in (a / "stable").iterdir()} | {p.name for p in (b / "stable").iterdir()})
    for name in names:
        left = stable_lines(a / "stable" / name)
        right = stable_lines(b / "stable" / name)
        delta = list(difflib.unified_diff(left, right, f"A/{name}", f"B/{name}", lineterm="", n=2))
        if delta:
            changed += 1
            print("\n".join(delta))
        else:
            print(f"  same  {name}")
    print("RESULT: identical" if changed == 0 else f"RESULT: {changed} item(s) differ")
    return 0 if changed == 0 else 1


def main(argv: list[str] | None = None) -> int:
    args = sys.argv[1:] if argv is None else argv
    commands: dict[str, tuple[int, Callable[..., int]]] = {"take": (1, take), "diff": (2, diff)}
    if not args or args[0] not in commands or len(args) - 1 != commands[args[0]][0]:
        print(__doc__, file=sys.stderr)
        return 2
    return commands[args[0]][1](*(Path(p) for p in args[1:]))


if __name__ == "__main__":
    sys.exit(main())
