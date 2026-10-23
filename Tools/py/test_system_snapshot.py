# SPDX-License-Identifier: LicenseRef-Proprietary
# Copyright (c) 2026 The Keyfeel Authors. All rights reserved.
"""Tests for system_snapshot.py's stable views and diff, on hand-made snapshots."""
from __future__ import annotations

import contextlib
import io
import json
import tempfile
import unittest
from pathlib import Path

import system_snapshot as ss

PROFILES = """Battery Power:
 lowpowermode         1
 displaysleep         2
AC Power:
 lowpowermode         0
 displaysleep         10
"""
ACTIVE_ONLY = """System-wide power settings:
Currently in use:
 lowpowermode         0
 displaysleep         10
"""


def snapshot(folder: Path, name: str, power: str, source: str, busy: int) -> Path:
    root = folder / name
    (root / "stable").mkdir(parents=True)
    meta = {"taken_utc": "2026-10-24T00:00:00+00:00", "boot_time": "1", "power_source": source}
    (root / "meta.json").write_text(json.dumps(meta))
    (root / "stable" / "power-settings.txt").write_text(power)
    device = [{"Product": "accel", "ReportInterval": 0, "IOServiceBusyTime": busy,
               "IORegistryEntryChildren": [{"IOObjectRetainCount": busy % 7}]}]
    (root / "stable" / "ioreg-AppleSPUHIDDevice.json").write_text(json.dumps(device))
    return root


def diff(a: Path, b: Path) -> tuple[int, str]:
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        code = ss.diff(a, b)
    return code, out.getvalue()


class SnapshotTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.folder = Path(self.tmp.name)

    def tearDown(self) -> None:
        self.tmp.cleanup()

    def test_counters_are_not_state(self) -> None:
        value = {"ReportInterval": 0, "IOServiceBusyTime": 5,
                 "IORegistryEntryChildren": [{"IOObjectRetainCount": 6, "QueueSize": 16384}]}
        self.assertEqual(ss.plain(value),
                         {"IORegistryEntryChildren": [{"QueueSize": 16384}], "ReportInterval": 0})

    def test_power_source(self) -> None:
        self.assertEqual(ss.power_source("Now drawing from 'Battery Power'\n -InternalBattery-0"),
                         "Battery Power")
        self.assertEqual(ss.power_source("Now drawing from 'AC Power'\n"), "AC Power")
        self.assertEqual(ss.power_source("<error>\n"), "")

    def test_profiles_are_recognized(self) -> None:
        self.assertTrue(ss.has_power_profiles(PROFILES.splitlines()))
        self.assertFalse(ss.has_power_profiles(ACTIVE_ONLY.splitlines()))
        self.assertFalse(ss.has_power_profiles([]))

    def test_unplugging_and_counters_are_identical(self) -> None:
        a = snapshot(self.folder, "a", PROFILES, "AC Power", busy=15343416)
        b = snapshot(self.folder, "b", PROFILES, "Battery Power", busy=15988958)
        code, out = diff(a, b)
        self.assertEqual(code, 0, out)
        self.assertIn("power source changed (AC Power -> Battery Power)", out)
        self.assertIn("RESULT: identical", out)

    def test_a_changed_profile_is_reported(self) -> None:
        a = snapshot(self.folder, "a", PROFILES, "AC Power", busy=1)
        b = snapshot(self.folder, "b", PROFILES.replace("displaysleep         10",
                                                        "displaysleep         30"),
                     "AC Power", busy=1)
        code, out = diff(a, b)
        self.assertEqual(code, 1)
        self.assertIn("+ displaysleep         30", out)

    def test_a_changed_driver_setting_is_reported(self) -> None:
        a = snapshot(self.folder, "a", PROFILES, "AC Power", busy=1)
        b = snapshot(self.folder, "b", PROFILES, "AC Power", busy=1)
        path = b / "stable" / "ioreg-AppleSPUHIDDevice.json"
        path.write_text(path.read_text().replace('"ReportInterval": 0', '"ReportInterval": 10000'))
        code, out = diff(a, b)
        self.assertEqual(code, 1)
        self.assertIn('"ReportInterval": 10000', out)

    def test_older_power_format_is_skipped_not_failed(self) -> None:
        a = snapshot(self.folder, "a", ACTIVE_ONLY, "", busy=1)
        b = snapshot(self.folder, "b", PROFILES, "AC Power", busy=1)
        code, out = diff(a, b)
        self.assertEqual(code, 0, out)
        self.assertIn("skip  power-settings.txt", out)


if __name__ == "__main__":
    unittest.main()
