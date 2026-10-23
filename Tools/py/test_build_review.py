# SPDX-License-Identifier: LicenseRef-Proprietary
# Copyright (c) 2026 The Keyfeel Authors. All rights reserved.
"""Tests for build_review.py (no network; synthesized audio only)."""
from __future__ import annotations

import json
import stat
import tempfile
import unittest
import zipfile
from pathlib import Path

import numpy as np
import soundfile as sf

import build_review as br
import render_demo as rd


def tone(ms: float, freq: float = 1500.0, rate: int = 44100) -> np.ndarray:
    t = np.arange(int(rate * ms / 1000)) / rate
    return 0.4 * np.sin(2 * np.pi * freq * t) * np.exp(-t / 0.015)


class SafetyTests(unittest.TestCase):
    def test_downloads_only_from_known_sites(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            for url in ("https://example.com/a.zip", "https://mechvibes.com.evil.net/a.zip",
                        "https://mechvibes.com/sound-packs/../../etc/passwd"):
                with self.assertRaises(SystemExit):
                    br.fetch(url, Path(tmp) / "x")

    def test_unzip_refuses_escapes_and_links(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            good = Path(tmp) / "good.zip"
            with zipfile.ZipFile(good, "w") as z:
                z.writestr("pack/config.json", "{}")
                z.writestr("__MACOSX/pack/._config.json", "junk")
                z.writestr("../escape.txt", "no")
            written = br.safe_unzip(good, Path(tmp) / "out")
            self.assertEqual([p.name for p in written], ["config.json"])
            self.assertFalse((Path(tmp) / "escape.txt").exists())
            link = Path(tmp) / "link.zip"
            with zipfile.ZipFile(link, "w") as z:
                info = zipfile.ZipInfo("evil")
                info.external_attr = (stat.S_IFLNK | 0o777) << 16
                z.writestr(info, "/etc/passwd")
            with self.assertRaises(SystemExit):
                br.safe_unzip(link, Path(tmp) / "out2")

    def test_slug(self) -> None:
        self.assertEqual(br.slug("Razor Green by Akira (Blackwidow Elite)"),
                         "Razor Green by Akira (Blackwidow Elite)")
        self.assertEqual(br.slug("a/b\\c:d"), "abcd")
        self.assertEqual(br.slug("///"), "pack")
        self.assertEqual(br.slug("Model_F_XT"), "Model F XT")
        self.assertEqual(br.slug("IBM Model F XT (Fractal_)"), "IBM Model F XT (Fractal)")


class TakesTests(unittest.TestCase):
    def test_community_sprite_and_multi_with_missing_files(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            single = Path(tmp) / "single" / "inner"
            single.mkdir(parents=True)
            sprite = np.concatenate([tone(100, f) for f in (1000, 2000, 3000)])
            sf.write(str(single / "s.wav"), sprite, 44100)
            (single / "config.json").write_text(json.dumps({
                "key_define_type": "single", "sound": "s.wav",
                "defines": {"30": [0, 100], "31": [100, 100], "57": [200, 100], "59": [0, 100]}}),
                encoding="utf-8")
            takes = br.community_takes(Path(tmp) / "single")
            self.assertEqual(len(takes[("alpha", "medium")]), 2)
            self.assertEqual(len(takes[("space", "medium")]), 1)
            self.assertAlmostEqual(len(takes[("alpha", "medium")][0]) / rd.RATE, 0.1, delta=0.002)

            multi = Path(tmp) / "multi"
            multi.mkdir()
            sf.write(str(multi / "a.wav"), tone(80), 44100)
            (multi / "config.json").write_text("﻿" + json.dumps({
                "key_define_type": "multi", "defines": {"30": "a.wav", "31": "missing.wav"}}),
                encoding="utf-8")
            takes = br.community_takes(multi)
            self.assertEqual(len(takes[("alpha", "medium")]), 1)
            self.assertEqual(br.community_takes(Path(tmp) / "nothing"), {})

    def test_folder_takes_skip_long_recordings(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            sf.write(str(folder / "key1.wav"), tone(90), 44100)
            sf.write(str(folder / "typing.wav"), np.zeros(44100 * 3), 44100)
            takes = br.folder_takes(folder)
            self.assertEqual(len(takes[("alpha", "medium")]), 1)

    def test_bandwidth_and_summary(self) -> None:
        takes = {("alpha", "medium"): [rd.to_rate(tone(120), 44100)], ("alpha", "up"): [tone(30)]}
        self.assertIsNotNone(br.bandwidth_khz(takes))
        self.assertEqual(br.summary(takes), "1 key sounds, 1 release sounds; groups: alpha")
        self.assertIsNone(br.bandwidth_khz({}))

    def test_review_sheet(self) -> None:
        rows = [{"section": "1", "folder": "01 A", "name": "A", "author": "x", "licence": "MIT",
                 "link": "https://a", "bw": 20.1, "sounds": "s"},
                {"section": "3", "folder": "B", "name": "B", "author": "y", "licence": "none",
                 "link": "https://b", "bw": None, "sounds": "t"}]
        sheet = br.review_md(rows)
        self.assertIn("## 1. In the app", sheet)
        self.assertIn("## 3. No licence: ask the author first", sheet)
        self.assertNotIn("## 2.", sheet)
        self.assertIn("| 01 A | [A](https://a) | x | MIT | 20.1 kHz | s |  |  |", sheet)


if __name__ == "__main__":
    unittest.main()
