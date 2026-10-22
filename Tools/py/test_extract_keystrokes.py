# SPDX-License-Identifier: LicenseRef-Proprietary
# Copyright (c) 2026 The Keyfeel Authors. All rights reserved.
"""Tests for extract_keystrokes.py on synthesized typing with a known answer (never real sounds)."""
from __future__ import annotations

import contextlib
import io
import json
import tempfile
import unittest
from pathlib import Path

import numpy as np
import soundfile as sf

import extract_keystrokes as ek

RATE = 48000


def click(rng: np.random.Generator, peak: float, freq: float, decay_s: float, length_s: float) -> np.ndarray:
    t = np.arange(int(RATE * length_s)) / RATE
    body = rng.standard_normal(len(t)) * 0.6 + np.sin(2 * np.pi * freq * t)
    body *= np.exp(-t / decay_s)
    return body * (peak / np.max(np.abs(body)))


def typing(seed: int = 3, keys: int = 24, knock: bool = True, rollover: bool = True
           ) -> tuple[np.ndarray, list[float]]:
    """Keystrokes every 0.4 s: a press, then a release 90 ms later and 8 dB quieter."""
    rng = np.random.default_rng(seed)
    x = rng.standard_normal(int(RATE * (keys * 0.4 + 1.5))) * 10 ** (-62 / 20)
    presses = []
    for k in range(keys):
        t = 0.3 + 0.4 * k
        level = 0.5 * rng.uniform(0.55, 1.0)
        for start, sound in ((t, click(rng, level, 2200.0, 0.008, 0.05)),
                             (t + 0.09, click(rng, level * 0.4, 3000.0, 0.004, 0.03))):
            i = int(start * RATE)
            x[i:i + len(sound)] += sound
        presses.append(t)
    end = 0.3 + 0.4 * keys
    if knock:  # A loud, dull desk knock: an outlier in level and colour.
        thud = click(rng, 0.95, 90.0, 0.03, 0.12)
        i = int((end + 0.2) * RATE)
        x[i:i + len(thud)] += thud
    if rollover:  # Press, press 60 ms later, then both releases: no isolated pair in it.
        for t, peak, freq in ((end + 0.70, 0.4, 2200.0), (end + 0.76, 0.4, 2200.0),
                              (end + 0.82, 0.16, 3000.0), (end + 0.88, 0.16, 3000.0)):
            sound = click(rng, peak, freq, 0.006, 0.04)
            i = int(t * RATE)
            x[i:i + len(sound)] += sound
    return x, presses


class ExtractTests(unittest.TestCase):
    def test_finds_presses_and_releases(self) -> None:
        x, truth = typing()
        result = ek.extract(x, RATE)
        self.assertGreaterEqual(result.events, 2 * len(truth))
        self.assertGreaterEqual(result.pairs, len(truth) - 1)
        starts = sorted(e.start / RATE for e in result.presses)
        for t in starts:  # Every kept press is a real press, within 2 ms.
            self.assertLess(min(abs(t - p) for p in truth), 0.002)
        self.assertGreaterEqual(len(starts), len(truth) - 3)
        for press in result.presses:
            self.assertLessEqual((press.end - press.start) / RATE, ek.MAX_TAKE_S)
        mean_press = np.mean([e.peak_db for e in result.presses])
        mean_release = np.mean([e.peak_db for e in result.releases])
        self.assertGreater(mean_press - mean_release, 5.0)

    def test_drops_knocks_and_rollover(self) -> None:
        x, truth = typing()
        result = ek.extract(x, RATE)
        end = 0.3 + 0.4 * len(truth)
        for e in result.presses + result.releases:
            self.assertLess(e.start / RATE, end, "the knock or the rollover was kept")

    def test_quiet_recording_yields_nothing(self) -> None:
        rng = np.random.default_rng(1)
        x = rng.standard_normal(RATE * 2) * 1e-3
        self.assertEqual(ek.extract(x, RATE).presses, [])

    def test_tiers_by_loudness_quartile(self) -> None:
        x, _ = typing(keys=32)
        result = ek.extract(x, RATE)
        chosen = ek.tiers(result.presses, 10, single=False)
        self.assertEqual(list(chosen), list(ek.TIERS))
        loudness = [np.mean([e.peak_db for e in chosen[t]]) for t in ek.TIERS]
        self.assertEqual(loudness, sorted(loudness))
        single = ek.tiers(result.presses, 5, single=True)
        self.assertEqual(list(single), ["medium"])
        self.assertEqual(len(single["medium"]), 5)
        few = ek.tiers(result.presses[:ek.MIN_FOR_TIERS - 1], 10, single=False)
        self.assertEqual(list(few), ["medium"])

    def test_writes_build_pack_layout_and_report(self) -> None:
        x, _ = typing(keys=32)
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "typing.flac"
            sf.write(str(src), np.stack([x, x], axis=1), RATE)  # Stereo in, mono out.
            out = Path(tmp) / "pack-src"
            (out / "alpha" / "slam").mkdir(parents=True)
            (out / "alpha" / "slam" / "stale.wav").write_bytes(b"old")
            (out / "SOURCES.md").write_text("keep me", encoding="utf-8")
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(ek.main([str(src), "--out", str(out), "--max-per-tier", "4"]), 0)
            self.assertFalse((out / "alpha" / "slam" / "stale.wav").exists())
            self.assertEqual((out / "SOURCES.md").read_text(encoding="utf-8"), "keep me")
            for tier in ek.TIERS:
                files = sorted((out / "alpha" / tier).glob("*.wav"))
                self.assertTrue(1 <= len(files) <= 4, tier)
                info = sf.info(str(files[0]))
                self.assertEqual((info.channels, info.samplerate, info.subtype), (1, RATE, "PCM_24"))
            self.assertTrue(list((out / "alpha" / "up").glob("*.wav")))
            report = json.loads((out / "extract-report.json").read_text(encoding="utf-8"))
            self.assertEqual(report["tiered_by"], "loudness quartile")
            self.assertGreater(report["press_snr_db_median"], 40.0)

    def test_no_keystrokes_fails_cleanly(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "hiss.wav"
            sf.write(str(src), np.random.default_rng(2).standard_normal(RATE) * 1e-3, RATE)
            with contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(ek.main([str(src), "--out", str(Path(tmp) / "out")]), 1)
            self.assertFalse((Path(tmp) / "out").exists())


if __name__ == "__main__":
    unittest.main()
