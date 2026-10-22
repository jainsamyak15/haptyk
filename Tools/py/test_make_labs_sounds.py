# SPDX-License-Identifier: LicenseRef-Proprietary
# Copyright (c) 2026 The Keyfeel Authors. All rights reserved.
"""Tests for make_labs_sounds.py and the committed boom it produced."""
from __future__ import annotations

import contextlib
import io
import tempfile
import unittest
import wave
from pathlib import Path

import numpy as np

import make_labs_sounds as mls

REPO = Path(__file__).resolve().parents[2]
COMMITTED = REPO / "App" / "Resources" / "Labs" / "boom.wav"
PACK_MAX_FILE_MS = 1500  # KF_PACK_MAX_FILE_MS in Core/include/kf/kf_config.h.


def read(path: Path) -> tuple[wave._wave_params, np.ndarray]:
    with wave.open(str(path), "rb") as f:
        params = f.getparams()
        data = np.frombuffer(f.readframes(params.nframes), dtype="<i2")
    return params, data.astype(np.float64) / 32767.0


def generate(folder: Path, *args: str) -> Path:
    out = folder / "boom.wav"
    with contextlib.redirect_stdout(io.StringIO()):
        mls.main(["--out", str(out), *args])
    return out


class BoomTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.folder = Path(self.tmp.name)

    def tearDown(self) -> None:
        self.tmp.cleanup()

    def test_format_fits_the_pack_rules(self) -> None:
        params, samples = read(generate(self.folder))
        self.assertEqual((params.nchannels, params.sampwidth, params.framerate), (1, 2, 48000))
        self.assertLessEqual(params.nframes * 1000 / params.framerate, PACK_MAX_FILE_MS)
        self.assertAlmostEqual(float(np.max(np.abs(samples))), mls.PEAK, delta=0.002)

    def test_starts_and_ends_without_a_click(self) -> None:
        _, samples = read(generate(self.folder))
        self.assertLess(abs(samples[0]), 0.01)
        self.assertEqual(samples[-1], 0.0)

    def test_energy_is_low_and_decays(self) -> None:
        params, samples = read(generate(self.folder))
        rate = params.framerate
        spectrum = np.abs(np.fft.rfft(samples[: rate // 4]))
        freqs = np.fft.rfftfreq(rate // 4, 1.0 / rate)
        self.assertLess(float(freqs[np.argmax(spectrum)]), 400.0)  # A boom, not a click.
        first = float(np.sqrt(np.mean(samples[: rate // 10] ** 2)))
        last = float(np.sqrt(np.mean(samples[-rate // 10:] ** 2)))
        self.assertGreater(first, 20.0 * last)

    def test_deterministic_and_seeded(self) -> None:
        a = generate(self.folder).read_bytes()
        b = generate(self.folder).read_bytes()
        self.assertEqual(a, b)
        self.assertNotEqual(a, generate(self.folder, "--seed", "12").read_bytes())
        params, _ = read(generate(self.folder, "--rate", "44100"))
        self.assertEqual(params.framerate, 44100)

    def test_committed_file_is_this_scripts_output(self) -> None:
        self.assertTrue(COMMITTED.is_file(), "run make_labs_sounds.py --out " + str(COMMITTED))
        self.assertEqual(COMMITTED.read_bytes(), generate(self.folder).read_bytes())


if __name__ == "__main__":
    unittest.main()
