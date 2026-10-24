# SPDX-License-Identifier: LicenseRef-Proprietary
# Copyright (c) 2026 The Keyfeel Authors. All rights reserved.
"""Tests for build_pack.py on synthesized source recordings (never real sounds)."""
from __future__ import annotations

import contextlib
import io
import json
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

import numpy as np
import soundfile as sf

import build_pack as bp

REPO = Path(__file__).resolve().parents[2]
KF_CLI = REPO / "build" / "debug" / "bin" / "kf-cli"
SOURCES_OK = """# Sources: Test

| Field | Value |
| --- | --- |
| Pack id | `com.keyfeel.test` |
| Licence | CC0 1.0 |
| Author | Test Author |
| URL or contract | https://example.com/recordings |
| Commercial use in an app allowed | Yes |
| Attribution required (shown in About) | None |
"""


def afconvert_supports_alac() -> bool:
    """Return whether this runner's afconvert can encode Apple Lossless."""
    if not Path(bp.AFCONVERT).exists():
        return False
    try:
        result = subprocess.run([bp.AFCONVERT, "-hf"], capture_output=True, text=True,
                                check=False)
    except OSError:
        return False
    return "alac" in result.stdout.lower() or "apple lossless" in result.stdout.lower()


ALAC_AVAILABLE = afconvert_supports_alac()


def click(rate: int, amplitude: float, freq: float, length_s: float = 0.06) -> np.ndarray:
    t = np.arange(int(rate * length_s)) / rate
    return amplitude * np.sin(2 * np.pi * freq * t) * np.exp(-t / 0.01)


def recording(rate: int, takes: int, amplitude: float, gap_s: float = 0.5) -> np.ndarray:
    """`takes` clicks separated by `gap_s` of silence, as the recording spec asks."""
    parts = [np.zeros(int(rate * 0.2))]
    for k in range(takes):
        parts += [click(rate, amplitude * (1.0 + 0.05 * k), 1800 + 100 * k), np.zeros(int(rate * gap_s))]
    return np.concatenate(parts)


class BuildPackTests(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory(dir=REPO / "build")
        self.root = Path(self._tmp.name)
        self.src = self.root / "src"
        self.out = self.root / "out"
        (self.src / "alpha" / "soft").mkdir(parents=True)
        (self.src / "alpha" / "hard").mkdir(parents=True)
        (self.src / "alpha" / "up").mkdir(parents=True)
        rate = 44100  # Sources at 44.1 kHz: the build resamples to 48 kHz.
        sf.write(str(self.src / "alpha" / "soft" / "takes.wav"), recording(rate, 3, 0.05), rate)
        sf.write(str(self.src / "alpha" / "hard" / "takes.wav"), recording(rate, 3, 0.6), rate)
        sf.write(str(self.src / "alpha" / "up" / "takes.wav"), recording(rate, 2, 0.1), rate)
        stereo = np.stack([recording(rate, 1, 0.3), recording(rate, 1, 0.3)], axis=1)
        sf.write(str(self.src / "space_down_medium.wav"), stereo, rate)  # Downmixed to mono.
        sf.write(str(self.src / "notes.wav"), recording(rate, 1, 0.3), rate)  # No group: skipped.
        (self.src / "SOURCES.md").write_text(SOURCES_OK, encoding="utf-8")

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def run_build(self, *extra: str) -> tuple[int, str]:
        err = io.StringIO()
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(err):
            code = bp.main(["--src", str(self.src), "--id", "com.keyfeel.test", "--name", "Test Pack",
                            "--switch-type", "linear", "--out", str(self.out), *extra])
        return code, err.getvalue()

    def manifest(self) -> dict:
        return json.loads((self.out / "Test-Pack.kfpack" / "pack.json").read_text())

    def test_splits_trims_normalizes_and_writes_the_manifest(self) -> None:
        code, err = self.run_build("--format", "wav")
        self.assertEqual(code, 0, err)
        self.assertIn("skipping notes.wav", err)
        m = self.manifest()
        self.assertEqual(m["format"], 1)
        self.assertEqual(m["license"], "CC0 1.0")
        self.assertEqual(m["author"], "Test Author")
        self.assertEqual(m["source_url"], "https://example.com/recordings")
        self.assertEqual(m["attribution"], "")
        self.assertEqual(m["derived"], [])
        alpha = m["groups"]["alpha"]
        self.assertEqual(len(alpha["down"]["soft"]), 3)
        self.assertEqual(len(alpha["down"]["hard"]), 3)
        self.assertEqual(len(alpha["up"]), 2)
        self.assertEqual(len(m["groups"]["space"]["down"]["medium"]), 1)
        pack = self.out / "Test-Pack.kfpack"
        self.assertTrue((pack / "SOURCES.md").is_file())
        peaks = {}
        for tier in ("soft", "hard"):
            for rel in alpha["down"][tier]:
                data, rate = sf.read(str(pack / rel))
                self.assertEqual(rate, 48000)
                self.assertEqual(data.ndim, 1)
                self.assertLessEqual(data.size, int(1.5 * rate))
                self.assertLess(abs(data[0]), 1e-3)  # The 2 ms fade-in.
                self.assertLess(abs(data[-1]), 1e-3)
                peaks.setdefault(tier, []).append(float(np.max(np.abs(data))))
        self.assertAlmostEqual(float(np.median(peaks["soft"])), bp.TARGET_PEAK["soft"], delta=0.01)
        self.assertAlmostEqual(float(np.median(peaks["hard"])), bp.TARGET_PEAK["hard"], delta=0.01)

    def test_derives_missing_tiers_and_marks_them(self) -> None:
        code, err = self.run_build("--format", "wav", "--derive-tiers")
        self.assertEqual(code, 0, err)
        m = self.manifest()
        self.assertEqual(m["derived"], ["alpha/medium", "alpha/slam", "space/hard", "space/slam",
                                        "space/soft"])
        for tier in bp.TIERS:
            self.assertIn(tier, m["groups"]["alpha"]["down"])
        pack = self.out / "Test-Pack.kfpack"
        loud = [float(np.max(np.abs(sf.read(str(pack / m["groups"]["alpha"]["down"][t][0]))[0])))
                for t in bp.TIERS]
        self.assertEqual(loud, sorted(loud))  # Loudness rises from soft to slam.

    def test_refuses_without_a_commercial_licence(self) -> None:
        sources = self.src / "SOURCES.md"
        for text, reason in (
            (SOURCES_OK.replace("| Yes |", "| No |"), "must say Yes"),
            (SOURCES_OK.replace("CC0 1.0", "CC-BY-NC 4.0"), "forbids commercial use"),
            (SOURCES_OK.replace("| Licence | CC0 1.0 |", "| Licence | |"), "Licence row is empty"),
        ):
            sources.write_text(text, encoding="utf-8")
            code, err = self.run_build("--format", "wav")
            self.assertEqual(code, 1)
            self.assertIn(reason, err)
        sources.unlink()
        code, err = self.run_build("--format", "wav")
        self.assertEqual(code, 1)
        self.assertIn("SOURCES.md is missing", err)

    def test_refuses_other_bad_input(self) -> None:
        shutil.rmtree(self.src / "alpha")
        code, err = self.run_build("--format", "wav")
        self.assertEqual(code, 1)
        self.assertIn("alpha key-down", err)
        code, err = self.run_build("--format", "wav", "--id", "nodots")
        self.assertEqual(code, 1)

    def test_never_overwrites_without_force(self) -> None:
        self.assertEqual(self.run_build("--format", "wav")[0], 0)
        code, err = self.run_build("--format", "wav")
        self.assertEqual(code, 1)
        self.assertIn("--force", err)
        self.assertEqual(self.run_build("--format", "wav", "--force")[0], 0)

    def test_classify(self) -> None:
        self.assertEqual(bp.classify(Path("alpha/soft/1.wav")), ("alpha", "down", "soft"))
        self.assertEqual(bp.classify(Path("Space_Down_Hard.wav")), ("space", "down", "hard"))
        self.assertEqual(bp.classify(Path("enter/release/2.wav")), ("enter", "up", "up"))
        self.assertIsNone(bp.classify(Path("misc/take.wav")))
        self.assertIsNone(bp.classify(Path("alpha/loud/1.wav")))

    @unittest.skipUnless(ALAC_AVAILABLE and KF_CLI.exists(),
                         "macOS with afconvert ALAC support and make tools")
    def test_caf_alac_pack_loads_in_the_app(self) -> None:
        code, err = self.run_build("--derive-tiers")  # The default: CAF/ALAC via afconvert.
        self.assertEqual(code, 0, err)
        pack = self.out / "Test-Pack.kfpack"
        self.assertTrue(all(p.suffix == ".caf" for p in (pack / "samples").iterdir()))
        done = subprocess.run([str(KF_CLI), "--validate", str(pack)], capture_output=True,
                              text=True, check=False)
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertIn("valid: Test Pack", done.stdout)
        self.assertIn("some tiers derived", done.stdout)


if __name__ == "__main__":
    unittest.main()
