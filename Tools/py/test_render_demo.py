# SPDX-License-Identifier: LicenseRef-Proprietary
# Copyright (c) 2026 The Keyfeel Authors. All rights reserved.
"""Tests for render_demo.py on synthesized packs (never real sounds)."""
from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

import numpy as np
import soundfile as sf

import render_demo as rd


def click(freq: float, peak: float = 0.5, ms: float = 40.0) -> np.ndarray:
    t = np.arange(int(rd.RATE * ms / 1000)) / rd.RATE
    return peak * np.sin(2 * np.pi * freq * t) * np.exp(-t / 0.01)


class RenderDemoTests(unittest.TestCase):
    def test_fallbacks_match_the_engine(self) -> None:
        a, b = [click(1000)], [click(2000)]
        takes = {("alpha", "medium"): a, ("alpha", "up"): b, ("space", "slam"): [click(300)]}
        self.assertIs(rd.resolve(takes, "alpha", "soft"), a)  # Nearest tier.
        self.assertIs(rd.resolve(takes, "enter", "hard"), a)  # No enter sounds: alpha's.
        self.assertIs(rd.resolve(takes, "enter", "up"), b)
        self.assertIs(rd.resolve(takes, "space", "soft"), takes[("space", "slam")])
        self.assertEqual(rd.resolve(takes, "space", "up"), [])  # Own downs, no release.
        self.assertEqual(rd.resolve({}, "alpha", "medium"), [])

    def test_key_positions(self) -> None:
        self.assertEqual(rd.key_of(" ")[0], "space")
        self.assertEqual(rd.key_of("\n")[0], "enter")
        self.assertEqual(rd.key_of("\b")[0], "backspace")
        self.assertLess(rd.key_of("q")[1], rd.key_of("p")[1])
        self.assertEqual(rd.key_of("A"), rd.key_of("a"))

    def test_plan_types_the_sentence_then_sweeps_the_tiers(self) -> None:
        events, length = rd.plan(seed=2)
        groups = {g for _, g, _, _, _ in events}
        self.assertTrue({"alpha", "space", "enter", "backspace", "modifier"} <= groups)
        times = [t for t, *_ in events]
        self.assertLess(max(times), length)
        sweep = [(tier, gain) for _, g, tier, gain, _ in events[-16:] if tier != "up"]
        self.assertEqual([t for t, _ in sweep], [t for t in rd.TIERS for _ in range(2)])
        gains = [g for _, g in sweep]
        self.assertEqual(gains, sorted(gains))  # Soft to slam, louder each tier.
        self.assertEqual(rd.plan(seed=2), rd.plan(seed=2))

    def test_render_is_stereo_bounded_and_deterministic(self) -> None:
        takes = {("alpha", t): [click(1500, p), click(1600, p)]
                 for t, p in zip(rd.TIERS, (0.1, 0.3, 0.6, 0.9))}
        takes[("alpha", "up")] = [click(3000, 0.2, 20)]
        out = rd.render(takes, seed=3)
        self.assertEqual(out.shape[1], 2)
        self.assertLessEqual(float(np.max(np.abs(out))), 1.0)
        self.assertAlmostEqual(len(out) / rd.RATE, rd.plan(seed=3)[1], delta=0.01)
        self.assertGreater(float(np.max(np.abs(out))), 0.1)
        self.assertTrue(np.array_equal(out, rd.render(takes, seed=3)))

    def test_kfpack_round_trip(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            pack = Path(tmp) / "T.kfpack"
            (pack / "samples").mkdir(parents=True)
            sf.write(str(pack / "samples" / "d.wav"), click(1200), 44100)  # 1920 frames at 44.1 kHz.
            sf.write(str(pack / "samples" / "u.wav"), click(2500, 0.2, 20), rd.RATE)
            (pack / "pack.json").write_text(json.dumps({"groups": {"alpha": {
                "down": {"medium": ["samples/d.wav"]}, "up": ["samples/u.wav"]}}}), encoding="utf-8")
            takes = rd.load_kfpack(pack)
            self.assertEqual(len(takes[("alpha", "medium")]), 1)
            self.assertAlmostEqual(len(takes[("alpha", "medium")][0]) / rd.RATE, 1920 / 44100, delta=0.0005)
            out = Path(tmp) / "demo.wav"
            self.assertEqual(rd.main([str(pack), "--out", str(out)]), 0)
            info = sf.info(str(out))
            self.assertEqual((info.channels, info.samplerate, info.subtype), (2, rd.RATE, "PCM_24"))


if __name__ == "__main__":
    unittest.main()
