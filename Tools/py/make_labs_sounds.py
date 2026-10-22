#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Proprietary
# Copyright (c) 2026 The Keyfeel Authors. All rights reserved.
"""Synthesize the Labs sounds: Big Enter's boom.

The boom is generated here, not recorded or sampled from anywhere, so the project owns it
outright (spec: Labs, "never copy a competitor's audio"). It is a sub-bass hit whose pitch
falls from 120 Hz to 45 Hz, saturated so that its harmonics carry on a MacBook's small
speakers, over a short filtered-noise thump. Output is deterministic for a given seed.

The committed App/Resources/Labs/boom.wav is this script's output; a test keeps them equal.
Only writes the file it is given.

Usage: make_labs_sounds.py --out App/Resources/Labs/boom.wav [--seed 11] [--rate 48000]
"""
from __future__ import annotations

import argparse
import sys
import wave
from pathlib import Path

import numpy as np

LENGTH_S = 0.9  # Under the pack limit of 1.5 s (KF_PACK_MAX_FILE_MS).
PEAK = 0.7  # About -3 dBFS: the engine adds the key's own sound on top.
START_HZ = 120.0
END_HZ = 45.0
GLIDE_S = 0.05  # Time constant of the pitch fall.
DECAY_S = 0.22  # Time constant of the body's decay.
DRIVE = 2.2  # Saturation: harmonics at 2x and 3x the fundamental.
THUMP_LEVEL = 0.35
THUMP_DECAY_S = 0.006
THUMP_ALPHA = 0.18  # One-pole low-pass on the noise: about 1.5 kHz at 48 kHz.
ATTACK_S = 0.004
FADE_S = 0.08


def lowpass(signal: np.ndarray, alpha: float) -> np.ndarray:
    """One-pole low-pass; alpha in (0, 1], higher is brighter."""
    out = np.empty_like(signal)
    acc = 0.0
    for i, x in enumerate(signal):
        acc += alpha * (x - acc)
        out[i] = acc
    return out


def boom(rate: int, seed: int) -> np.ndarray:
    n = int(rate * LENGTH_S)
    t = np.arange(n) / rate
    # Phase of a sine whose frequency glides exponentially from START_HZ to END_HZ.
    sweep = START_HZ - END_HZ
    phase = 2.0 * np.pi * (END_HZ * t + sweep * GLIDE_S * (1.0 - np.exp(-t / GLIDE_S)))
    body = np.tanh(DRIVE * np.sin(phase)) / np.tanh(DRIVE)
    body *= np.exp(-t / DECAY_S)
    rng = np.random.default_rng(seed)
    thump = lowpass(rng.standard_normal(n), THUMP_ALPHA) * np.exp(-t / THUMP_DECAY_S)
    thump /= max(float(np.max(np.abs(thump))), 1e-9)
    signal = body + THUMP_LEVEL * thump
    signal *= np.minimum(1.0, t / ATTACK_S)  # No click at the start.
    fade = int(rate * FADE_S)
    signal[n - fade:] *= 0.5 * (1.0 + np.cos(np.linspace(0.0, np.pi, fade)))  # Ends at zero.
    return signal * (PEAK / float(np.max(np.abs(signal))))


def write_wav(path: Path, samples: np.ndarray, rate: int) -> None:
    pcm = np.clip(np.round(samples * 32767.0), -32768, 32767).astype("<i2")
    with wave.open(str(path), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(rate)
        out.writeframes(pcm.tobytes())


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--out", type=Path, required=True, help="output .wav file")
    parser.add_argument("--seed", type=int, default=11)
    parser.add_argument("--rate", type=int, default=48000, choices=(44100, 48000))
    args = parser.parse_args(argv)

    args.out.parent.mkdir(parents=True, exist_ok=True)
    write_wav(args.out, boom(args.rate, args.seed), args.rate)
    print(f"make_labs_sounds: {args.out.stat().st_size / 1024:.0f} KiB -> {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
