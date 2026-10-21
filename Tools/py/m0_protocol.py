#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Proprietary
# Copyright (c) 2026 The Keyfeel Authors. All rights reserved.
"""Guides the owner through the Milestone 0 test protocol (docs/SPEC.md).

For one surface (desk or lap) it records, with Tools/sensor-log:
  rest     10 s with hands off
  taps     20 soft and 20 hard taps on the home row, space bar, number row and both edges
  typing   30 s of normal typing

Each step writes logs/<hw.model>_macos-<version>/<surface>-<step>.csv with labels that
`analyze.py report` groups by. Finished steps are skipped, so an interrupted session
resumes where it stopped (--redo records them again). Nothing typed during a recording
reaches the shell: the terminal is silenced and its input discarded afterwards.

Usage:
  m0_protocol.py --surface desk            run every step
  m0_protocol.py --surface lap --only rest,home-soft,home-hard
  m0_protocol.py --list                    show the steps
  m0_protocol.py ... -- --tap hid          pass extra flags to sensor-log
"""
from __future__ import annotations

import argparse
import contextlib
import platform
import re
import signal
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Iterator

REPO = Path(__file__).resolve().parents[2]
SENSOR_LOG = REPO / "build" / "debug" / "bin" / "sensor-log"
# The accelerometer as found by hid-dump (docs/SENSOR_NOTES.md, "Device").
ACCEL_MATCH = ["--vendor", "0x05ac", "--product", "0x8104", "--usage-page", "0xff00", "--usage", "3"]
TAPS = 20
TAP_TIMEOUT_S = 90
COUNTDOWN_S = 3

ROWS = (
    ("home", "a home-row key such as F or J"),
    ("space", "the space bar"),
    ("number", "a number-row key such as 5"),
    ("left-edge", "a key at the left edge such as Q or A"),
    ("right-edge", "a key at the right edge such as P or the apostrophe"),
)
FORCES = (
    ("soft", "softly, as lightly as still types a character"),
    ("hard", "hard, the way you hit keys when typing forcefully"),
)


@dataclass(frozen=True)
class Step:
    name: str  # Unique within a surface, e.g. "home-soft".
    tags: dict[str, str]
    seconds: int
    keys: int | None  # Stop after this many key-downs.
    instruction: str


def steps_for(surface: str) -> list[Step]:
    steps = [
        Step(
            "rest",
            {"surface": surface, "force": "rest"},
            10,
            None,
            "Hands off: don't touch the Mac or what it rests on for 10 seconds.",
        )
    ]
    for row, where in ROWS:
        for force, how in FORCES:
            steps.append(
                Step(
                    f"{row}-{force}",
                    {"surface": surface, "row": row, "force": force},
                    TAP_TIMEOUT_S,
                    TAPS,
                    f"Tap {where} {how}, {TAPS} times, about once a second. "
                    "Keep other fingers off the keyboard.",
                )
            )
    steps.append(
        Step(
            "typing",
            {"surface": surface, "force": "typing"},
            30,
            None,
            "Type normally for 30 seconds (any text; it is discarded, never recorded).",
        )
    )
    return steps


def label(tags: dict[str, str]) -> str:
    return ",".join(f"{k}={v}" for k, v in tags.items())


def machine_dir() -> str:
    model = subprocess.run(["sysctl", "-n", "hw.model"], capture_output=True, text=True).stdout
    version = platform.mac_ver()[0] or "unknown"
    return re.sub(r"[^A-Za-z0-9,._-]", "_", f"{model.strip() or 'unknown'}_macos-{version}")


@contextlib.contextmanager
def silenced_terminal() -> Iterator[None]:
    """No echo while recording, then discard whatever was typed so it never reaches the shell."""
    if not sys.stdin.isatty():
        yield
        return
    import termios

    fd = sys.stdin.fileno()
    saved = termios.tcgetattr(fd)
    quiet = termios.tcgetattr(fd)
    quiet[3] &= ~(termios.ECHO | termios.ICANON)
    termios.tcsetattr(fd, termios.TCSANOW, quiet)
    try:
        yield
    finally:
        termios.tcflush(fd, termios.TCIFLUSH)
        termios.tcsetattr(fd, termios.TCSANOW, saved)


def footer_value(path: Path, key: str) -> str | None:
    try:
        lines = path.read_text(encoding="utf-8").splitlines()[-16:]
    except OSError:
        return None
    for line in lines:
        if line.startswith(f"# {key}="):
            return line.split("=", 1)[1]
    return None


def record(step: Step, out: Path, sensor_log: Path, extra: list[str], redo: bool) -> bool:
    print(f"\n== {step.name} ==\n{step.instruction}")
    with contextlib.suppress(EOFError):
        input("Press Return when ready (Ctrl-C quits)... ")
    for n in range(COUNTDOWN_S, 0, -1):
        print(f"  {n}...", end="", flush=True)
        time.sleep(1)
    print("  go")
    cmd = [str(sensor_log), *ACCEL_MATCH, "--seconds", str(step.seconds), "--label",
           label(step.tags), "--out", str(out), *extra]
    if step.keys:
        cmd += ["--stop-after-keys", str(step.keys)]
    if redo:
        cmd.append("--force")
    # Ctrl-C ends the recording (sensor-log handles it) without killing this runner. A Python
    # handler, unlike SIG_IGN, resets to the default in the child at exec.
    previous = signal.signal(signal.SIGINT, lambda *_: None)
    try:
        with silenced_terminal():
            code = subprocess.run(cmd, check=False).returncode
    finally:
        signal.signal(signal.SIGINT, previous)
    if code != 0:
        print(f"sensor-log failed with exit status {code}; see the message above.")
        return False
    reports = footer_value(out, "end.reports")
    keys = footer_value(out, "end.keydown")
    print(f"  saved {out.relative_to(REPO)}: {reports} reports, {keys} key-downs")
    if reports == "0":
        print(
            "  The sensor delivered no reports, so nothing else can be measured yet.\n"
            "  See docs/SENSOR_NOTES.md, 'Open questions for the owner'."
        )
        return False
    if step.keys and keys is not None and int(keys) < step.keys:
        print(f"  Only {keys} of {step.keys} taps were recorded; use --redo --only {step.name}.")
    return True


def main(argv: list[str] | None = None) -> int:
    argv = list(sys.argv[1:] if argv is None else argv)
    extra: list[str] = []
    if "--" in argv:
        split = argv.index("--")
        argv, extra = argv[:split], argv[split + 1 :]
    parser = argparse.ArgumentParser(
        prog="m0_protocol.py",
        description=__doc__.splitlines()[0],
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="\n".join(__doc__.splitlines()[2:]),
    )
    parser.add_argument("--surface", choices=("desk", "lap"), default="desk")
    parser.add_argument("--only", help="comma-separated step names (see --list)")
    parser.add_argument("--redo", action="store_true", help="record finished steps again")
    parser.add_argument("--list", action="store_true", help="list the steps and exit")
    parser.add_argument("--out", type=Path, default=REPO / "logs", help="default: logs/")
    parser.add_argument("--sensor-log", type=Path, default=SENSOR_LOG)
    args = parser.parse_args(argv)

    steps = steps_for(args.surface)
    if args.list:
        for step in steps:
            print(f"  {step.name:<17} {step.instruction}")
        return 0
    if args.only:
        wanted = [s.strip() for s in args.only.split(",") if s.strip()]
        unknown = sorted(set(wanted) - {s.name for s in steps})
        if unknown:
            parser.error(f"unknown steps: {', '.join(unknown)} (see --list)")
        steps = [s for s in steps if s.name in wanted]
    if not args.sensor_log.exists():
        parser.error(f"{args.sensor_log} not found: run `make tools` first")

    out_dir = (args.out / machine_dir()).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    print(f"Milestone 0 protocol, surface: {args.surface}. Logs go to {out_dir}.")
    try:
        for step in steps:
            out = out_dir / f"{args.surface}-{step.name}.csv"
            if out.exists() and not args.redo:
                print(f"  skip {step.name}: {out.name} exists (--redo records it again)")
                continue
            if not record(step, out, args.sensor_log, extra, args.redo):
                return 1
    except KeyboardInterrupt:
        print("\nStopped. Run the same command again to resume.")
        return 130
    print(
        "\nDone. Analyze with:\n"
        f"  Tools/py/.venv/bin/python Tools/py/analyze.py report {out_dir.relative_to(REPO)}/*.csv"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
