#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Proprietary
# Copyright (c) 2026 The Keyfeel Authors. All rights reserved.
"""Render a listening demo of a sound pack, played the way Keyfeel's engine plays it.

Types a sentence at a natural pace (with Backspace, Shift and Enter), each key at its own pan,
a different take each time and never the same one twice in a row, press and release sounds at
the engine's levels (release 0.6, level 0.7 + 0.3 x force), then hits one key twice at each
force tier from soft to slam. Mixed with the engine's soft clipper, written as 48 kHz stereo
24-bit WAV. For people to judge packs by ear; never shipped.

Usage: render_demo.py PACK.kfpack --out demo.wav [--seed 1]
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
import soundfile as sf
from scipy.signal import resample_poly

RATE = 48000
TIERS = ("soft", "medium", "hard", "slam")
TIER_FORCE = {"soft": 0.0, "medium": 1 / 3, "hard": 2 / 3, "slam": 1.0}
RELEASE_GAIN = 0.6  # KF_RELEASE_GAIN
SOFT_CLIP = 0.8  # KF_SOFT_CLIP_THRESHOLD
TEXT = "The quick brown fox jumps over the lazy dog. Typing feels good."
ROWS = ("1234567890-=", "qwertyuiop[]", "asdfghjkl;'", "zxcvbnm,./")  # For the pan by column.

Takes = dict[tuple[str, str], list[np.ndarray]]  # (group, tier or "up") -> mono takes


def decode(path: Path) -> tuple[np.ndarray, int]:
    """Mono float64; CAF/ALAC through macOS's afconvert, everything else through soundfile."""
    try:
        data, rate = sf.read(str(path), dtype="float64", always_2d=True)
    except (RuntimeError, sf.LibsndfileError):
        with tempfile.TemporaryDirectory() as tmp:
            wav = Path(tmp) / "x.wav"
            subprocess.run(["/usr/bin/afconvert", "-f", "WAVE", "-d", "LEF32", str(path), str(wav)],
                           check=True, capture_output=True)
            data, rate = sf.read(str(wav), dtype="float64", always_2d=True)
    return data.mean(axis=1), int(rate)


def to_rate(x: np.ndarray, rate: int) -> np.ndarray:
    if rate == RATE:
        return x
    from math import gcd
    g = gcd(rate, RATE)
    return resample_poly(x, RATE // g, rate // g)


def load_kfpack(pack: Path) -> Takes:
    manifest = json.loads((pack / "pack.json").read_text(encoding="utf-8"))
    takes: Takes = {}
    for group, entry in manifest["groups"].items():
        for tier, files in entry.get("down", {}).items():
            takes[(group, tier)] = [to_rate(*decode(pack / f)) for f in files]
        if entry.get("up"):
            takes[(group, "up")] = [to_rate(*decode(pack / f)) for f in entry["up"]]
    return takes


def resolve(takes: Takes, group: str, tier: str) -> list[np.ndarray]:
    """The engine's fallbacks (ADR-0012): nearest tier, softer on a tie; then alpha's sounds."""
    for g in (group, "alpha"):
        if tier == "up":
            if (g, "up") in takes:
                return takes[(g, "up")]
            if any((g, t) in takes for t in TIERS):
                return []  # The group has its own key-downs but no release sound.
            continue
        order = sorted(TIERS, key=lambda t: (abs(TIERS.index(t) - TIERS.index(tier)), TIERS.index(t)))
        for t in order:
            if (g, t) in takes:
                return takes[(g, t)]
    return []


def key_of(ch: str) -> tuple[str, float]:
    """Key group and pan (-1 left .. +1 right) for a character."""
    if ch == " ":
        return "space", 0.0
    if ch == "\n":
        return "enter", 0.75
    if ch == "\b":
        return "backspace", 0.85
    if ch == "\t":
        return "tab", -0.8
    if ch == "\x0e":  # Shift.
        return "modifier", -0.9
    c = ch.lower()
    for row in ROWS:
        if c in row:
            return "alpha", -0.9 + 1.8 * row.index(c) / max(1, len(row) - 1)
    return "alpha", 0.0


class Mixer:
    def __init__(self, seconds: float, seed: int) -> None:
        self.out = np.zeros((int(seconds * RATE), 2))
        self.rng = np.random.default_rng(seed)
        self.last: dict[int, int] = {}

    def play(self, sounds: list[np.ndarray], t: float, gain: float, pan: float) -> None:
        if not sounds:
            return
        key = id(sounds)
        choices = [i for i in range(len(sounds)) if i != self.last.get(key)] or [0]
        i = int(self.rng.choice(choices))
        self.last[key] = i
        x = sounds[i] * gain
        angle = (np.clip(pan, -1.0, 1.0) + 1.0) * np.pi / 4  # Constant power, as the engine.
        start = int(t * RATE)
        end = min(len(self.out), start + len(x))
        if end > start:
            self.out[start:end, 0] += x[: end - start] * np.cos(angle)
            self.out[start:end, 1] += x[: end - start] * np.sin(angle)

    def master(self) -> np.ndarray:
        a = np.abs(self.out)
        knee = 1.0 - SOFT_CLIP
        over = np.maximum(a - SOFT_CLIP, 0.0)
        return np.sign(self.out) * (np.minimum(a, SOFT_CLIP) + knee * over / (knee + over))


Event = tuple[float, str, str, float, float]  # time s, group, tier or "up", gain, pan


def plan(seed: int = 1) -> tuple[list[Event], float]:
    """The demo's keystrokes and its length: the sentence, then one key per tier, soft first."""
    rng = np.random.default_rng(seed)
    script = "\x0e" + TEXT[:30] + "\b\b" + TEXT[28:] + "\n"
    events: list[Event] = []
    t = 0.4
    for ch in script:
        group, pan = key_of(ch)
        force = TIER_FORCE["medium"] + rng.uniform(-0.08, 0.08)
        events.append((t, group, "medium", 0.7 + 0.3 * force, pan))
        hold = rng.uniform(0.07, 0.11) + (0.12 if ch in " \n" else 0.0)
        events.append((t + hold, group, "up", RELEASE_GAIN, pan))
        t += rng.uniform(0.12, 0.2) + (0.35 if ch in ".\n" else 0.0)
    t += 0.8
    for tier in TIERS:  # One key, twice per tier: the force range.
        for _ in range(2):
            events.append((t, "alpha", tier, 0.7 + 0.3 * TIER_FORCE[tier], 0.0))
            events.append((t + 0.09, "alpha", "up", RELEASE_GAIN, 0.0))
            t += 0.42
        t += 0.25
    return events, t + 0.6


def render(takes: Takes, seed: int = 1) -> np.ndarray:
    events, length = plan(seed)
    mix = Mixer(length + 1.0, seed)
    for t, group, tier, gain, pan in events:
        mix.play(resolve(takes, group, tier), t, gain, pan)
    return mix.master()[: int(length * RATE)]


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("pack", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--seed", type=int, default=1)
    args = parser.parse_args(argv)
    takes = load_kfpack(args.pack)
    if not resolve(takes, "alpha", "medium"):
        print(f"render_demo: {args.pack} has no alpha key-down sound", file=sys.stderr)
        return 1
    args.out.parent.mkdir(parents=True, exist_ok=True)
    sf.write(str(args.out), render(takes, args.seed), RATE, subtype="PCM_24")
    return 0


if __name__ == "__main__":
    sys.exit(main())
