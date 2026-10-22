#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Proprietary
# Copyright (c) 2026 The Keyfeel Authors. All rights reserved.
"""Cut a licensed typing recording into single keystrokes for build_pack.py.

Free libraries mostly offer continuous typing, not the take-by-take sessions of the recording
spec (Appendix B). This finds each keystroke's press and release in such a recording and keeps
only clean ones. A keystroke is two sound events, 45 to 200 ms apart, with at least 80 ms of
quiet before and after them: a release never comes before its press, so the first is the press.
Rollover (a second press before the first release) and fast typing fail the quiet test and are
skipped. Kept keystrokes are well above the noise floor and typical in level and colour (desk
knocks, mouse clicks and the odd space bar are dropped as outliers). Several transients within
45 ms (fingertip, bottom-out, plate ring) are one event. Takes are cut from the recording after
a 30 Hz high-pass that removes DC offset and rumble. It writes them where build_pack.py looks:

  <out>/alpha/<tier>/NN.wav   presses, split into the four tiers by loudness quartile: the
                              typist's own variation, recorded rather than generated
  <out>/alpha/up/NN.wav       releases
  <out>/extract-report.json   what was found and kept, for SOURCES.md

With too few presses for four tiers it writes them all as medium (build them with
--derive-tiers). Every keystroke is labelled alpha: the other key groups fall back to alpha's
sounds (ADR-0012). Reads only the input, writes only inside --out.

Usage: extract_keystrokes.py IN --out Packs/sources/<id> [--max-per-tier 10] [--single-tier]
       IN is anything soundfile reads (WAV, FLAC, AIFF, Ogg Vorbis, MP3), or any format macOS's
       afconvert reads (M4A/AAC), which is converted to WAV in a temporary folder first.
"""
from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np
import soundfile as sf
from scipy.signal import butter, sosfiltfilt

TIERS = ("soft", "medium", "hard", "slam")
HOP_S = 0.0005  # Envelope resolution.
PREROLL_S = 0.001
MIN_EVENT_GAP_S = 0.045  # Onsets closer than this are one event: a press is several transients.
REARM_DB = 10.0  # The envelope must fall this far below the threshold between events.
PAIR_MIN_S = 0.045  # Press to release.
PAIR_MAX_S = 0.200
RELEASE_LOUDER_DB = 3.0  # A release clearly louder than its press is more likely a press.
QUIET_BEFORE_S = 0.080  # Nothing may sound this long before a press...
QUIET_AFTER_S = 0.080  # ...or after its release.
MAX_TAKE_S = 0.300
MIN_PRESS_S = 0.040  # Keeps a switch's low body ("thock"), unless the release comes sooner.
BODY_END_DB = 6.0  # An event ends when its full-band level falls this close to the floor.
MIN_SNR_DB = 20.0
OUTLIER_Z = 2.5  # Robust z-score of level or spectral centroid that drops an event.
MIN_FOR_TIERS = 16  # Presses needed to split into four tiers (4 takes each).
AFCONVERT = "/usr/bin/afconvert"
HIGHPASS_HZ = 30.0  # Removes DC offset and rumble, which would thump at a take's edges.


@dataclass
class Settings:
    """Per-recording tuning: clicky switches need a wider merge, deliberate taps a longer hold."""
    merge_s: float = MIN_EVENT_GAP_S
    max_hold_s: float = PAIR_MAX_S
    quiet_s: float = QUIET_BEFORE_S


@dataclass
class Event:
    start: int  # Sample index of the onset (with preroll).
    end: int  # Exclusive: where the take is cut (at least MIN_PRESS_S, never into the next).
    decay: int  # Where the sound itself has decayed into the noise: for the quiet tests.
    peak_db: float
    snr_db: float
    centroid_hz: float = 0.0


@dataclass
class Result:
    rate: int
    presses: list[Event] = field(default_factory=list)
    releases: list[Event] = field(default_factory=list)
    events: int = 0
    pairs: int = 0
    noise_floor_db: float = 0.0


def highpass(x: np.ndarray, rate: int) -> np.ndarray:
    """Zero-phase high-pass at HIGHPASS_HZ: nothing audible changes, the transients keep their
    timing."""
    sos = butter(2, HIGHPASS_HZ, btype="highpass", fs=rate, output="sos")
    return sosfiltfilt(sos, x)


def load(path: Path) -> tuple[np.ndarray, int]:
    """Mono float64 and its rate. Formats soundfile can't read go through afconvert."""
    try:
        data, rate = sf.read(str(path), dtype="float64", always_2d=True)
    except (RuntimeError, sf.LibsndfileError):
        with tempfile.TemporaryDirectory() as tmp:
            wav = Path(tmp) / "converted.wav"
            subprocess.run([AFCONVERT, "-f", "WAVE", "-d", "LEF32", str(path), str(wav)],
                           check=True, capture_output=True)
            data, rate = sf.read(str(wav), dtype="float64", always_2d=True)
    return data.mean(axis=1), int(rate)


def envelope_db(x: np.ndarray, rate: int) -> tuple[np.ndarray, np.ndarray, int]:
    """Peak envelopes in dB, one value per hop: of a pre-emphasised signal, where transients
    stand out (onsets), and of the full band over 3 ms, where a switch's low body lasts (ends)."""
    emphasised = np.empty_like(x)
    emphasised[0] = x[0]
    emphasised[1:] = x[1:] - 0.95 * x[:-1]
    hop = max(1, int(rate * HOP_S))
    frames = len(x) // hop
    onset = np.abs(emphasised[: frames * hop]).reshape(frames, hop).max(axis=1)
    body = np.abs(x[: frames * hop]).reshape(frames, hop).max(axis=1)
    width = max(1, int(0.003 / HOP_S))
    padded = np.concatenate([body, np.zeros(width - 1)])
    body = np.lib.stride_tricks.sliding_window_view(padded, width).max(axis=1)
    return 20.0 * np.log10(onset + 1e-9), 20.0 * np.log10(body + 1e-9), hop


def spectral_centroid(x: np.ndarray, rate: int) -> float:
    if len(x) < 16:
        return 0.0
    spectrum = np.abs(np.fft.rfft(x * np.hanning(len(x))))
    freqs = np.fft.rfftfreq(len(x), 1.0 / rate)
    total = float(spectrum.sum())
    return float((spectrum * freqs).sum() / total) if total > 0.0 else 0.0


def find_events(x: np.ndarray, rate: int, settings: Settings = Settings()) -> tuple[list[Event], float]:
    env, body, hop = envelope_db(x, rate)
    floor = float(np.percentile(env, 20))
    body_floor = float(np.percentile(body, 20))
    top = float(np.percentile(env, 99.9))
    threshold = floor + max(15.0, 0.4 * (top - floor))
    events: list[Event] = []
    armed = True
    min_gap = int(settings.merge_s / HOP_S)
    last_onset = -min_gap
    k = 0
    while k < len(env):
        if armed and env[k] >= threshold and k - last_onset >= min_gap:
            onset = k
            # End: the full-band level has decayed into the noise, capped at MAX_TAKE_S.
            limit = min(len(env), onset + int(MAX_TAKE_S / HOP_S))
            end = onset + int(0.005 / HOP_S)
            while end < limit and body[end] > body_floor + BODY_END_DB:
                end += 1
            decay_s = min(len(x), end * hop)
            end = min(limit, max(end, onset + int(MIN_PRESS_S / HOP_S)))
            start_s = max(0, onset * hop - int(PREROLL_S * rate))
            end_s = min(len(x), end * hop)
            peak = float(np.max(np.abs(x[start_s:end_s]))) if end_s > start_s else 0.0
            peak_db = 20.0 * np.log10(peak + 1e-9)
            events.append(Event(start_s, end_s, decay_s, peak_db, float(env[onset:end].max()) - floor))
            last_onset = onset
            armed = False
        elif not armed and env[k] < threshold - REARM_DB:
            armed = True
        k += 1
    # An event never runs into the next one.
    for a, b in zip(events, events[1:]):
        a.end = min(a.end, max(a.start + 1, b.start - int(0.002 * rate)))
        a.decay = min(a.decay, a.end)
    return events, floor


def pair(events: list[Event], rate: int, settings: Settings = Settings()) -> list[tuple[Event, Event]]:
    pairs = []
    i = 0
    while i + 1 < len(events):
        press, release = events[i], events[i + 1]
        gap = (release.start - press.start) / rate
        before = (press.start - events[i - 1].decay) / rate if i > 0 else 1.0
        after = (events[i + 2].start - release.decay) / rate if i + 2 < len(events) else 1.0
        if (max(PAIR_MIN_S, settings.merge_s) <= gap <= settings.max_hold_s
                and release.peak_db - press.peak_db <= RELEASE_LOUDER_DB
                and before >= settings.quiet_s and after >= settings.quiet_s):
            pairs.append((press, release))
            i += 2
        else:
            i += 1
    return pairs


def robust_z(values: np.ndarray) -> np.ndarray:
    median = np.median(values)
    mad = np.median(np.abs(values - median)) * 1.4826
    return np.zeros_like(values) if mad == 0.0 else (values - median) / mad


def typical(group: list[Event]) -> list[Event]:
    """Drops outliers in level or colour: knocks, mouse clicks, a stray space bar."""
    if len(group) < 4:
        return group
    level = robust_z(np.array([e.peak_db for e in group]))
    colour = robust_z(np.array([e.centroid_hz for e in group]))
    return [e for e, a, b in zip(group, level, colour) if abs(a) <= OUTLIER_Z and abs(b) <= OUTLIER_Z]


def extract(x: np.ndarray, rate: int, settings: Settings = Settings()) -> Result:
    events, floor = find_events(x, rate, settings)
    result = Result(rate=rate, events=len(events), noise_floor_db=floor)
    pairs = [(p, r) for p, r in pair(events, rate, settings) if p.snr_db >= MIN_SNR_DB]
    result.pairs = len(pairs)
    for p, r in pairs:
        p.centroid_hz = spectral_centroid(x[p.start:p.end], rate)
        r.centroid_hz = spectral_centroid(x[r.start:r.end], rate)
    result.presses = typical([p for p, _ in pairs])
    result.releases = typical([r for _, r in pairs])
    return result


def tiers(presses: list[Event], max_per_tier: int, single: bool) -> dict[str, list[Event]]:
    """Loudness quartiles, softest first; each tier keeps its most typical takes."""
    if single or len(presses) < MIN_FOR_TIERS:
        ordered = sorted(presses, key=lambda e: e.peak_db)
        middle = len(ordered) // 2
        half = max_per_tier // 2
        return {"medium": ordered[max(0, middle - half): middle - half + max_per_tier]}
    ordered = sorted(presses, key=lambda e: e.peak_db)
    out = {}
    for n, tier in enumerate(TIERS):
        chunk = ordered[n * len(ordered) // 4:(n + 1) * len(ordered) // 4]
        median = np.median([e.centroid_hz for e in chunk])
        out[tier] = sorted(sorted(chunk, key=lambda e: abs(e.centroid_hz - median))[:max_per_tier],
                           key=lambda e: e.start)
    return out


def write_take(path: Path, x: np.ndarray, rate: int, event: Event) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    take = np.concatenate([x[event.start:event.end], np.zeros(int(0.3 * rate))])
    sf.write(str(path), take, rate, subtype="PCM_24")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0],
                                     formatter_class=argparse.RawDescriptionHelpFormatter,
                                     epilog=__doc__.split("\n\n", 1)[1])
    parser.add_argument("input", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--max-per-tier", type=int, default=10)
    parser.add_argument("--single-tier", action="store_true",
                        help="all presses as medium (for --derive-tiers)")
    parser.add_argument("--merge-ms", type=float, default=MIN_EVENT_GAP_S * 1000,
                        help="transients closer than this are one event (clicky: 60-75)")
    parser.add_argument("--max-hold-ms", type=float, default=PAIR_MAX_S * 1000,
                        help="longest press to release (deliberate taps: up to 400)")
    parser.add_argument("--quiet-ms", type=float, default=QUIET_BEFORE_S * 1000,
                        help="quiet needed before a press and after its release")
    args = parser.parse_args(argv)
    settings = Settings(args.merge_ms / 1000, args.max_hold_ms / 1000, args.quiet_ms / 1000)

    x, rate = load(args.input)
    result = extract(x, rate, settings)  # Detection sees the recording as it is...
    x = highpass(x, rate)  # ...the takes are cut from the cleaned signal.
    if not result.presses:
        print(f"extract_keystrokes: no clean keystroke found in {args.input}", file=sys.stderr)
        return 1
    for folder in [args.out / "alpha" / t for t in (*TIERS, "up")]:
        if folder.exists():
            shutil.rmtree(folder)  # Only the folders this script writes.
    chosen = tiers(result.presses, args.max_per_tier, args.single_tier)
    for tier, takes in chosen.items():
        for n, event in enumerate(takes, 1):
            write_take(args.out / "alpha" / tier / f"{n:02d}.wav", x, rate, event)
    releases = sorted(result.releases, key=lambda e: abs(e.centroid_hz - np.median(
        [r.centroid_hz for r in result.releases])))[: args.max_per_tier]
    for n, event in enumerate(sorted(releases, key=lambda e: e.start), 1):
        write_take(args.out / "alpha" / "up" / f"{n:02d}.wav", x, rate, event)

    snr = [e.snr_db for e in result.presses]
    report = {
        "input": args.input.name,
        "rate": rate,
        "seconds": round(len(x) / rate, 2),
        "events": result.events,
        "clean_pairs": result.pairs,
        "presses_kept": len(result.presses),
        "releases_kept": len(result.releases),
        "tiers": {t: len(v) for t, v in chosen.items()},
        "release_takes": len(releases),
        "press_snr_db_median": round(float(np.median(snr)), 1),
        "press_ms_median": round(float(np.median([(e.end - e.start) / rate * 1000 for e in result.presses])), 1),
        "tiered_by": "loudness quartile" if len(chosen) == 4 else "single tier (derive the rest)",
        "settings_ms": {"merge": args.merge_ms, "max_hold": args.max_hold_ms, "quiet": args.quiet_ms},
    }
    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / "extract-report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report))
    return 0


if __name__ == "__main__":
    sys.exit(main())
