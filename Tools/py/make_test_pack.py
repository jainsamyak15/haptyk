#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Proprietary
# Copyright (c) 2026 The Keyfeel Authors. All rights reserved.
"""Synthesize the "Test Click" pack for debug builds and pipeline tests.

The sounds are generated, not recorded: a filtered noise burst plus a short
resonant ping, louder and brighter as force rises. Output is deterministic for
a given seed. Only writes inside --out.

Usage: make_test_pack.py --out build/packs [--seed 7] [--rate 48000]
"""
from __future__ import annotations

import argparse
import json
import shutil
import sys
import wave
from pathlib import Path

import numpy as np

PACK_DIR_NAME = "Test-Click.kfpack"
TAKES = 3
RELEASE_TAKES = 3

# group -> (ping frequency in Hz, body length in ms)
GROUPS: dict[str, tuple[float, float]] = {
    "alpha": (2200.0, 55.0),
    "space": (900.0, 90.0),
    "enter": (1300.0, 75.0),
    "backspace": (1600.0, 70.0),
    "modifier": (1900.0, 60.0),
    "tab": (1700.0, 60.0),
}

# tier -> (peak amplitude, brightness 0..1)
TIERS: dict[str, tuple[float, float]] = {
    "soft": (0.22, 0.25),
    "medium": (0.42, 0.45),
    "hard": (0.68, 0.70),
    "slam": (0.95, 1.00),
}


def lowpass(signal: np.ndarray, alpha: float) -> np.ndarray:
    """One-pole low-pass; alpha in (0, 1], higher is brighter."""
    out = np.empty_like(signal)
    acc = 0.0
    for i, x in enumerate(signal):
        acc += alpha * (x - acc)
        out[i] = acc
    return out


def synth(rng: np.random.Generator, rate: int, freq: float, length_ms: float,
          amplitude: float, brightness: float) -> np.ndarray:
    n = int(rate * length_ms / 1000.0)
    t = np.arange(n) / rate
    body = lowpass(rng.standard_normal(n), 0.05 + 0.55 * brightness)
    body *= np.exp(-t / (0.006 + 0.004 * (1.0 - brightness)))
    detune = 1.0 + rng.uniform(-0.03, 0.03)
    ping = np.sin(2.0 * np.pi * freq * detune * t) * np.exp(-t / 0.012)
    signal = body + ping * (0.35 + 0.4 * brightness)
    signal *= np.minimum(1.0, t / 0.0005)  # 0.5 ms attack: no click at the start
    fade = min(n, int(rate * 0.005))
    signal[n - fade:] *= np.linspace(1.0, 0.0, fade)  # 5 ms tail fade
    peak = float(np.max(np.abs(signal)))
    return signal * (amplitude / peak if peak > 0.0 else 0.0)


def write_wav(path: Path, samples: np.ndarray, rate: int) -> None:
    pcm = np.clip(np.round(samples * 32767.0), -32768, 32767).astype("<i2")
    with wave.open(str(path), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(rate)
        out.writeframes(pcm.tobytes())


def build(out_dir: Path, seed: int, rate: int) -> Path:
    rng = np.random.default_rng(seed)
    pack_dir = out_dir / PACK_DIR_NAME
    if pack_dir.exists():
        shutil.rmtree(pack_dir)  # Only the pack this script owns.
    samples_dir = pack_dir / "samples"
    samples_dir.mkdir(parents=True)

    groups: dict[str, dict] = {}
    for group, (freq, length_ms) in GROUPS.items():
        down: dict[str, list[str]] = {}
        for tier, (amplitude, brightness) in TIERS.items():
            files = []
            for take in range(1, TAKES + 1):
                name = f"{group}_down_{tier}_{take}.wav"
                write_wav(samples_dir / name,
                          synth(rng, rate, freq, length_ms, amplitude, brightness), rate)
                files.append(f"samples/{name}")
            down[tier] = files
        up = []
        for take in range(1, RELEASE_TAKES + 1):
            name = f"{group}_up_{take}.wav"
            write_wav(samples_dir / name, synth(rng, rate, freq * 1.4, 30.0, 0.18, 0.8), rate)
            up.append(f"samples/{name}")
        groups[group] = {"down": down, "up": up}

    manifest = {
        "format": 1,
        "id": "com.keyfeel.test-click",
        "name": "Test Click",
        "switch_type": "other",
        "author": "Keyfeel",
        "license": "proprietary",
        "version": "1.0.0",
        "source_url": None,
        "attribution": "",
        "derived": [],
        "groups": groups,
    }
    (pack_dir / "pack.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return pack_dir


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--out", type=Path, required=True, help="output folder")
    parser.add_argument("--seed", type=int, default=7)
    parser.add_argument("--rate", type=int, default=48000, choices=(44100, 48000))
    args = parser.parse_args(argv)

    pack_dir = build(args.out, args.seed, args.rate)
    files = sorted((pack_dir / "samples").glob("*.wav"))
    size = sum(f.stat().st_size for f in files)
    print(f"make_test_pack: {len(files)} files, {size / 1024:.0f} KiB -> {pack_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
