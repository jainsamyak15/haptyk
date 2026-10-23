#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Proprietary
# Copyright (c) 2026 The Keyfeel Authors. All rights reserved.
"""Build a .kfpack from licensed source recordings (spec: "Sound pack format", Appendix B).

Reads Packs/sources/<pack-id>/: a SOURCES.md (from Packs/SOURCES.template.md) and recordings.
Each recording's group, tier and direction come from its path, for example
`alpha/soft/take-3.wav`, `space_down_hard.wav` or `enter/up/1.wav` (groups: alpha, space,
enter, backspace, modifier, tab; tiers: soft, medium, hard, slam; `up` or `release` marks key-up
sounds). A file may hold several takes separated by silence, as the recording spec asks
(0.5 s between takes).

For every take: split on silence, trim, add a 2 ms fade-in and a 5 ms fade-out, cap at 1.5 s,
resample to --rate, normalize per tier so loudness rises from soft to slam, and encode CAF/ALAC
with macOS's built-in afconvert (or 24-bit WAV with --format wav). --derive-tiers generates
missing tiers from the nearest recorded one (lower gain and a gentle low-pass toward soft, more
gain and a slight high-shelf toward slam) and marks them derived. Writes pack.json and copies
SOURCES.md into the pack.

Refuses to build without a SOURCES.md that says commercial use in an app is allowed, or with a
non-commercial (NC) licence. Writes only inside --out, and replaces an existing pack only with
--force. Never shipped.

Usage: build_pack.py --src Packs/sources/<id> --id com.keyfeel.<id> --name NAME
                     --switch-type TYPE [--author A] [--version V] [--source-url URL]
                     [--derive-tiers] [--max-takes N] [--rate 48000] [--format caf|wav]
                     [--out App/Resources/Packs] [--force]
"""
from __future__ import annotations

import argparse
import json
import math
import re
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

import numpy as np

GROUPS = ("alpha", "space", "enter", "backspace", "modifier", "tab")
TIERS = ("soft", "medium", "hard", "slam")
SWITCH_TYPES = ("linear", "tactile", "clicky", "topre", "buckling", "other")
AUDIO_SUFFIXES = {".wav", ".aif", ".aiff", ".flac", ".caf"}

TARGET_PEAK = {"soft": 0.25, "medium": 0.45, "hard": 0.70, "slam": 0.95, "up": 0.30}
MAX_TAKE_S = 1.5  # KF_PACK_MAX_FILE_MS
FADE_IN_S = 0.002
FADE_OUT_S = 0.005
SILENCE_DB = -40.0  # Relative to the file's peak.
MIN_GAP_S = 0.08  # Quieter stretches shorter than this stay inside one take.
MIN_TAKE_S = 0.012
PREROLL_S = 0.001
TAIL_S = 0.02
MAX_FILES = 400  # KF_PACK_MAX_FILES
MAX_DECODED_BYTES = 8 * 1024 * 1024  # KF_PACK_MAX_DECODED_BYTES (16-bit mono)
AFCONVERT = "/usr/bin/afconvert"


class BuildError(RuntimeError):
    """A pack that must not be built, with a reason for the pack author."""


# ---------------------------------------------------------------------------- licence


@dataclass
class Sources:
    licence: str
    commercial: bool
    attribution: str
    author: str
    url: str


def read_sources(src: Path) -> Sources:
    """The SOURCES.md table (Packs/SOURCES.template.md): licence and the commercial-use row."""
    path = src / "SOURCES.md"
    if not path.is_file():
        raise BuildError(f"{path} is missing: copy Packs/SOURCES.template.md and fill it in")
    rows: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) == 2 and cells[0] and not set(cells[0]) <= set("-: "):
            rows[cells[0].lower()] = cells[1]
    commercial = rows.get("commercial use in an app allowed", "").lower().startswith("yes")
    licence = rows.get("licence", rows.get("license", "")).strip()
    if not licence:
        raise BuildError(f"{path}: the Licence row is empty")
    if re.search(r"\bNC\b|non-?commercial", licence, re.IGNORECASE):
        raise BuildError(f"{path}: '{licence}' forbids commercial use (Appendix B: skip CC-BY-NC)")
    if not commercial:
        raise BuildError(f"{path}: 'Commercial use in an app allowed' must say Yes")
    attribution = rows.get("attribution required (shown in about)", "")
    if attribution.lower() in ("", "no", "none", "-"):
        attribution = ""
    return Sources(licence, commercial, attribution, rows.get("author", ""),
                   rows.get("url or contract", ""))


# ---------------------------------------------------------------------------- audio


@dataclass
class Take:
    group: str
    direction: str  # "down" or "up"
    tier: str  # a tier, or "up"
    samples: np.ndarray  # mono float64 at the pack rate
    derived: bool = False
    source: str = ""


def classify(relative: Path) -> tuple[str, str, str] | None:
    """(group, direction, tier) from a path such as alpha/soft/1.wav or space_up_2.wav."""
    tokens = [t for t in re.split(r"[/\\_\-. ]+", str(relative.with_suffix("")).lower()) if t]
    group = next((t for t in tokens if t in GROUPS), None)
    if group is None:
        return None
    if "up" in tokens or "release" in tokens:
        return group, "up", "up"
    tier = next((t for t in tokens if t in TIERS), None)
    return (group, "down", tier) if tier else None


def load_mono(path: Path, rate: int) -> np.ndarray:
    import soundfile as sf
    from scipy.signal import resample_poly

    data, source_rate = sf.read(str(path), always_2d=True, dtype="float64")
    mono = data.mean(axis=1)
    if source_rate != rate:
        g = math.gcd(int(source_rate), rate)
        mono = resample_poly(mono, rate // g, int(source_rate) // g)
    return mono


def runs(mask: np.ndarray) -> list[tuple[int, int]]:
    """[start, end) of every run of True."""
    padded = np.concatenate([[False], mask, [False]])
    edges = np.flatnonzero(np.diff(padded.astype(np.int8)))
    return list(zip(edges[0::2], edges[1::2]))


def split_takes(x: np.ndarray, rate: int) -> list[np.ndarray]:
    """Takes separated by silence: trimmed, faded and capped at MAX_TAKE_S."""
    from scipy.ndimage import maximum_filter1d

    peak = float(np.max(np.abs(x))) if x.size else 0.0
    if peak <= 0.0:
        return []
    envelope = maximum_filter1d(np.abs(x), size=max(1, int(0.005 * rate)))
    active = envelope > peak * 10.0 ** (SILENCE_DB / 20.0)
    merged: list[list[int]] = []
    for start, end in runs(active):
        if merged and start - merged[-1][1] < MIN_GAP_S * rate:
            merged[-1][1] = end
        else:
            merged.append([start, end])
    takes = []
    for start, end in merged:
        if end - start < MIN_TAKE_S * rate:
            continue
        a = max(0, start - int(PREROLL_S * rate))
        b = min(x.size, end + int(TAIL_S * rate), a + int(MAX_TAKE_S * rate))
        takes.append(apply_fades(x[a:b].copy(), rate))
    return takes


def apply_fades(take: np.ndarray, rate: int) -> np.ndarray:
    fade_in = min(take.size, int(FADE_IN_S * rate))
    fade_out = min(take.size, int(FADE_OUT_S * rate))
    if fade_in:
        take[:fade_in] *= np.linspace(0.0, 1.0, fade_in, endpoint=False)
    if fade_out:
        take[-fade_out:] *= np.linspace(1.0, 0.0, fade_out)
    return take


def normalize(takes: list[Take], target: float) -> None:
    """One gain for a tier: its median take peak lands on the target, none clips."""
    peaks = [float(np.max(np.abs(t.samples))) for t in takes]
    median = float(np.median(peaks))
    if median <= 0.0:
        return
    gain = target / median
    gain = min(gain, 0.99 / max(peaks))
    for t in takes:
        t.samples = t.samples * gain


def derive(source: list[Take], tier: str, rate: int) -> list[Take]:
    """A missing tier from the nearest recorded one: softer is quieter and darker, harder is
    louder and brighter. Gains are set later by normalize(); only the timbre changes here."""
    from scipy.signal import butter, sosfilt

    soft_side = TIERS.index(tier) < TIERS.index(source[0].tier)
    if soft_side:
        sos = butter(2, 3500.0, btype="lowpass", fs=rate, output="sos")
        shape = lambda x: sosfilt(sos, x)  # noqa: E731
    else:
        sos = butter(2, 3000.0, btype="highpass", fs=rate, output="sos")
        shape = lambda x: x + 0.35 * sosfilt(sos, x)  # noqa: E731 - a gentle high shelf
    return [
        Take(t.group, "down", tier, apply_fades(shape(t.samples), rate), True, t.source)
        for t in source
    ]


# ---------------------------------------------------------------------------- build


def collect(src: Path, rate: int, max_takes: int) -> list[Take]:
    takes: list[Take] = []
    files = sorted(p for p in src.rglob("*") if p.suffix.lower() in AUDIO_SUFFIXES and p.is_file()
                   and "raw" not in p.relative_to(src).parts)  # raw/ holds downloads, never takes.
    if not files:
        raise BuildError(f"{src}: no recordings (.wav, .aif, .flac, .caf)")
    for path in files:
        if path.is_symlink():
            raise BuildError(f"{path} is a symbolic link")
        kind = classify(path.relative_to(src))
        if kind is None:
            print(f"build_pack: skipping {path.relative_to(src)}: no group/tier in its path",
                  file=sys.stderr)
            continue
        group, direction, tier = kind
        for samples in split_takes(load_mono(path, rate), rate):
            takes.append(Take(group, direction, tier, samples, False, path.name))
    by_slot: dict[tuple[str, str], list[Take]] = {}
    for t in takes:
        by_slot.setdefault((t.group, t.tier), []).append(t)
    limited = [t for slot in by_slot.values() for t in slot[:max_takes]]
    return limited


def plan(takes: list[Take], derive_tiers: bool, rate: int) -> list[Take]:
    if not any(t.group == "alpha" and t.direction == "down" for t in takes):
        raise BuildError("no alpha key-down recording: every pack needs one (spec)")
    result = list(takes)
    if derive_tiers:
        for group in GROUPS:
            recorded = {t.tier for t in takes if t.group == group and t.direction == "down"}
            if not recorded:
                continue  # The app falls back to alpha.
            for tier in TIERS:
                if tier in recorded:
                    continue
                nearest = min(recorded, key=lambda r: (abs(TIERS.index(r) - TIERS.index(tier)),
                                                       TIERS.index(r)))
                result += derive([t for t in takes if t.group == group and t.tier == nearest],
                                 tier, rate)
    for group in GROUPS:
        for tier in (*TIERS, "up"):
            slot = [t for t in result if t.group == group and t.tier == tier]
            if slot:
                normalize(slot, TARGET_PEAK[tier])
    return result


def encode(take: Take, out: Path, rate: int, fmt: str, scratch: Path) -> None:
    import soundfile as sf

    pcm = np.clip(take.samples, -1.0, 1.0)
    if fmt == "wav":
        sf.write(str(out), pcm, rate, subtype="PCM_24")
        return
    wav = scratch / (out.stem + ".wav")
    sf.write(str(wav), pcm, rate, subtype="PCM_24")
    done = subprocess.run([AFCONVERT, "-f", "caff", "-d", "alac", str(wav), str(out)],
                          capture_output=True, text=True, check=False)
    if done.returncode != 0:
        raise BuildError(f"afconvert failed for {out.name}: {done.stderr.strip()}")


def build(args: argparse.Namespace) -> Path:
    src: Path = args.src
    sources = read_sources(src)
    if not re.fullmatch(r"[A-Za-z0-9.-]{3,100}", args.id) or "." not in args.id:
        raise BuildError(f"--id {args.id!r} must be reverse-DNS, like com.keyfeel.nk-cream")
    if args.format == "caf" and not Path(AFCONVERT).exists():
        raise BuildError("afconvert not found (macOS only): use --format wav")
    takes = plan(collect(src, args.rate, args.max_takes), args.derive_tiers, args.rate)
    if len(takes) > MAX_FILES:
        raise BuildError(f"{len(takes)} sounds; a pack holds at most {MAX_FILES} (use --max-takes)")
    decoded = sum(t.samples.size for t in takes) * 2
    if decoded > MAX_DECODED_BYTES:
        raise BuildError(f"{decoded // 1024} KB decoded; the limit is {MAX_DECODED_BYTES // 1024} KB")

    pack = args.out / (re.sub(r"[^A-Za-z0-9]+", "-", args.name).strip("-") + ".kfpack")
    if pack.exists():
        if not args.force:
            raise BuildError(f"{pack} exists: use --force to replace it")
        shutil.rmtree(pack)  # Only this one pack, inside --out.
    samples = pack / "samples"
    samples.mkdir(parents=True)
    groups: dict[str, dict] = {}
    derived: set[str] = set()
    counters: dict[tuple[str, str], int] = {}
    suffix = ".caf" if args.format == "caf" else ".wav"
    with tempfile.TemporaryDirectory(dir=args.out) as scratch:
        for t in sorted(takes, key=lambda t: (GROUPS.index(t.group), (*TIERS, "up").index(t.tier))):
            key = (t.group, t.tier)
            counters[key] = counters.get(key, 0) + 1
            name = (f"{t.group}_up_{counters[key]}" if t.direction == "up"
                    else f"{t.group}_down_{t.tier}_{counters[key]}") + suffix
            encode(t, samples / name, args.rate, args.format, Path(scratch))
            entry = groups.setdefault(t.group, {})
            if t.direction == "up":
                entry.setdefault("up", []).append(f"samples/{name}")
            else:
                entry.setdefault("down", {}).setdefault(t.tier, []).append(f"samples/{name}")
            if t.derived:
                derived.add(f"{t.group}/{t.tier}")
    manifest = {
        "format": 1,
        "id": args.id,
        "name": args.name,
        "switch_type": args.switch_type,
        "author": args.author or sources.author or "Unknown",
        "license": sources.licence,
        "version": args.version,
        "source_url": args.source_url or (sources.url if sources.url.startswith("http") else None),
        "attribution": sources.attribution,
        "derived": sorted(derived),
        "groups": groups,
    }
    (pack / "pack.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    shutil.copyfile(src / "SOURCES.md", pack / "SOURCES.md")
    return pack


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0],
                                     formatter_class=argparse.RawDescriptionHelpFormatter,
                                     epilog="\n".join(__doc__.splitlines()[2:]))
    parser.add_argument("--src", type=Path, required=True)
    parser.add_argument("--id", required=True)
    parser.add_argument("--name", required=True)
    parser.add_argument("--switch-type", choices=SWITCH_TYPES, required=True)
    parser.add_argument("--author", default="")
    parser.add_argument("--version", default="1.0.0")
    parser.add_argument("--source-url", default=None)
    parser.add_argument("--derive-tiers", action="store_true")
    parser.add_argument("--max-takes", type=int, default=10)
    parser.add_argument("--rate", type=int, default=48000, choices=(44100, 48000))
    parser.add_argument("--format", choices=("caf", "wav"), default="caf")
    parser.add_argument("--out", type=Path, default=Path("App/Resources/Packs"))
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args(argv)
    try:
        args.out.mkdir(parents=True, exist_ok=True)
        pack = build(args)
    except BuildError as exc:
        print(f"build_pack: {exc}", file=sys.stderr)
        return 1
    files = list((pack / "samples").iterdir())
    size = sum(f.stat().st_size for f in files)
    print(f"build_pack: {len(files)} files, {size / 1024:.0f} KiB -> {pack}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
