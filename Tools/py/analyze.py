#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Proprietary
# Copyright (c) 2026 The Keyfeel Authors. All rights reserved.
"""Analyze Milestone 0 sensor logs written by Tools/sensor-log.

Commands:
  inspect LOG            byte statistics, candidate layouts, clock units and sample rate
  report LOG [LOG ...]   per-keystroke peaks and delays, the go/no-go gate, noise floors,
                         PRE/POST windows and permissions, as Markdown for
                         docs/SENSOR_NOTES.md (--keys lists every keystroke; --json)
  plot LOG --out PNG     deviation over time with key-down and key-up markers
  fixture LOG --out CSV  a small excerpt for Tests/Fixtures/sensor/

Layouts are written TYPEx3@OFFSET, for example int32x3@6: three little-endian values of
TYPE (int16, int32 or float32) starting at byte OFFSET of each report. A 6-axis report
shows up as two 3-axis candidates, one per triple.

Labels: sensor-log --label takes comma-separated key=value tags. `report` groups keystrokes
by surface (desk, lap), row (number, top, home, bottom, space, left-edge, right-edge) and
force (rest, soft, hard, typing). Tools/py/m0_protocol.py writes these labels for you.

Force is measured exactly as Core/src/velocity/kf_velocity_rt.c computes it: the baseline
is the mean of each axis over [t - PRE - BASELINE, t - PRE), and the peak is the largest
|sample - baseline| over [t - PRE, t + POST]. The current PRE, POST and BASELINE are read
from Core/include/kf/kf_config.h so the analysis cannot drift from the core.
"""
from __future__ import annotations

import argparse
import json
import math
import os
import re
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any, Sequence

import numpy as np

REPO = Path(__file__).resolve().parents[2]
CONFIG_H = REPO / "Core" / "include" / "kf" / "kf_config.h"
# Matplotlib caches fonts under ~/.matplotlib by default; dev tools stay inside the repo.
os.environ.setdefault("MPLCONFIGDIR", str(REPO / "build" / "mplconfig"))

FORMAT_VERSION = "1"
COLUMNS = "t_ns,event,src_ts,data"
MS = 1_000_000
STANDARD_GRAVITY = 9.80665
DTYPES = {"int16": "<i2", "int32": "<i4", "float32": "<f4"}

SEARCH_PRE_NS = 30 * MS  # Discovery windows: deliberately wider than any plausible PRE/POST.
SEARCH_POST_NS = 60 * MS
ISOLATION_NS = 150 * MS  # A keystroke is isolated when no other key-down came this soon before.
RESPONSE_NS = 40 * MS  # Span after key-down scored when ranking layouts by key response.
MIN_GROUP = 5  # Keystrokes needed before a group's percentiles are reported.
POST_BUDGET_NS = 10 * MS  # Gate: POST must not exceed this (docs/SPEC.md, Milestone 0).
GATE_ROWS = ("home", "space")
ROW_ORDER = ("number", "top", "home", "bottom", "space", "left-edge", "right-edge")


class LogError(ValueError):
    """A file that is not a valid sensor-log CSV, or one that cannot be analyzed."""


# ---------------------------------------------------------------------------- config


def read_config(path: Path = CONFIG_H) -> dict[str, int]:
    """Window constants from kf_config.h, with the spec's placeholders as a fallback."""
    values = {
        "KF_WINDOW_PRE_NS": 5 * MS,
        "KF_WINDOW_POST_NS": 8 * MS,
        "KF_BASELINE_NS": 50 * MS,
        "KF_SENSOR_NOMINAL_HZ": 1000,
    }
    try:
        text = path.read_text(encoding="utf-8")
    except OSError:
        return values
    for name in values:
        match = re.search(rf"#define\s+{name}\s+(\d+)", text)
        if match:
            values[name] = int(match.group(1))
    return values


# ---------------------------------------------------------------------------- parsing


@dataclass
class SensorLog:
    path: Path
    meta: dict[str, str]
    report_arrival_ns: np.ndarray  # int64, the kf_time clock
    report_src: np.ndarray  # int64, raw source timestamps
    reports: np.ndarray  # uint8, shape (n, report_length); the most common length only
    other_length_reports: int
    keydown_arrival_ns: np.ndarray
    keydown_src: np.ndarray
    keyup_arrival_ns: np.ndarray
    keyup_src: np.ndarray

    @property
    def name(self) -> str:
        return self.path.name

    @property
    def tags(self) -> dict[str, str]:
        return parse_tags(self.meta.get("label", ""))

    @property
    def timebase(self) -> tuple[int, int] | None:
        numer, _, denom = self.meta.get("mach_timebase", "").partition("/")
        if numer.isdigit() and denom.isdigit() and int(denom) > 0:
            return int(numer), int(denom)
        return None

    @property
    def report_length(self) -> int:
        return int(self.reports.shape[1]) if self.reports.size else 0


def parse_tags(label: str) -> dict[str, str]:
    """'surface=desk,row=home' -> {'surface': 'desk', 'row': 'home'}; bare words map to ''."""
    tags: dict[str, str] = {}
    for item in label.split(","):
        key, _, value = item.partition("=")
        if key.strip():
            tags[key.strip()] = value.strip()
    return tags


def _int64(values: list[int], path: Path, what: str) -> np.ndarray:
    try:
        return np.array(values, dtype=np.int64)
    except OverflowError:
        raise LogError(f"{path}: {what} out of the int64 range") from None


def read_log(path: Path) -> SensorLog:
    """Parses a sensor-log CSV. Raises LogError with file and line on malformed input."""
    path = Path(path)
    meta: dict[str, str] = {}
    by_length: dict[int, tuple[list[int], list[int], list[str]]] = {}
    keys: dict[str, tuple[list[int], list[int]]] = {"KEYDOWN": ([], []), "KEYUP": ([], [])}
    columns_seen = False
    try:
        handle = path.open(encoding="utf-8")
    except OSError as exc:
        raise LogError(f"{path}: {exc.strerror or exc}") from exc
    with handle:
        for lineno, raw in enumerate(handle, 1):
            line = raw.rstrip("\r\n")
            if not line:
                continue
            if line[0] == "#":
                key, sep, value = line[1:].partition("=")
                if sep:
                    meta[key.strip()] = value.strip()
                continue
            if not columns_seen:
                if line != COLUMNS:
                    raise LogError(f"{path}:{lineno}: expected the column header {COLUMNS!r}")
                columns_seen = True
                continue
            fields = line.split(",")
            if len(fields) != 4:
                raise LogError(f"{path}:{lineno}: expected 4 fields, found {len(fields)}")
            t_text, event, src_text, data = fields
            if not (t_text.isdigit() and src_text.isdigit()):
                raise LogError(f"{path}:{lineno}: timestamps must be unsigned integers")
            if event == "REPORT":
                if not data or len(data) % 2:
                    raise LogError(f"{path}:{lineno}: report data must be whole bytes of hex")
                bucket = by_length.setdefault(len(data) // 2, ([], [], []))
                bucket[0].append(int(t_text))
                bucket[1].append(int(src_text))
                bucket[2].append(data)
            elif event in keys:
                if data:
                    raise LogError(f"{path}:{lineno}: key rows carry no data")
                keys[event][0].append(int(t_text))
                keys[event][1].append(int(src_text))
            else:
                raise LogError(f"{path}:{lineno}: unknown event {event!r}")

    if meta.get("keyfeel sensor-log format") != FORMAT_VERSION:
        raise LogError(f"{path}: not a sensor-log format {FORMAT_VERSION} file")
    if not columns_seen:
        raise LogError(f"{path}: no column header")

    reports = np.zeros((0, 0), dtype=np.uint8)
    report_t: list[int] = []
    report_src: list[int] = []
    other = 0
    if by_length:
        length = max(by_length, key=lambda n: len(by_length[n][0]))
        report_t, report_src, hexes = by_length[length]
        try:
            blob = bytes.fromhex("".join(hexes))
        except ValueError:
            raise LogError(f"{path}: report data is not hexadecimal") from None
        reports = np.frombuffer(blob, dtype=np.uint8).reshape(len(hexes), length)
        other = sum(len(bucket[0]) for n, bucket in by_length.items() if n != length)

    return SensorLog(
        path=path,
        meta=meta,
        report_arrival_ns=_int64(report_t, path, "report time"),
        report_src=_int64(report_src, path, "report source timestamp"),
        reports=reports,
        other_length_reports=other,
        keydown_arrival_ns=_int64(keys["KEYDOWN"][0], path, "key time"),
        keydown_src=_int64(keys["KEYDOWN"][1], path, "key source timestamp"),
        keyup_arrival_ns=_int64(keys["KEYUP"][0], path, "key time"),
        keyup_src=_int64(keys["KEYUP"][1], path, "key source timestamp"),
    )


# ---------------------------------------------------------------------------- statistics


def pct(values: Sequence[float] | np.ndarray, q: float) -> float | None:
    data = np.asarray(values, dtype=np.float64)
    return float(np.percentile(data, q)) if data.size else None


def summary(values: Sequence[float] | np.ndarray) -> dict[str, float] | None:
    data = np.asarray(values, dtype=np.float64)
    if not data.size:
        return None
    p5, p10, p50, p90, p95 = np.percentile(data, [5, 10, 50, 90, 95])
    return {
        "n": int(data.size),
        "min": float(data.min()),
        "p5": float(p5),
        "p10": float(p10),
        "p50": float(p50),
        "p90": float(p90),
        "p95": float(p95),
        "max": float(data.max()),
    }


CEIL_SLACK_MS = 0.02  # Round up to whole ms, ignoring overshoot this small (clock jitter).


def ceil_ms(ns: float) -> int:
    return int(math.ceil(ns / MS - CEIL_SLACK_MS)) * MS


# ---------------------------------------------------------------------------- clocks


@dataclass
class ClockFit:
    unit: str  # "mach ticks", "ns", "us", "ms" or "unknown"
    ns_per_unit: float  # fitted slope of arrival time against the source timestamp
    same_clock: bool  # converted source times share the arrival clock's epoch
    delay_ns: dict[str, float] | None  # arrival minus converted source time


def _clock_units(timebase: tuple[int, int] | None) -> list[tuple[str, float]]:
    units = [("ns", 1.0), ("us", 1e3), ("ms", 1e6)]
    if timebase is not None and timebase[0] != timebase[1]:
        units.insert(0, ("mach ticks", timebase[0] / timebase[1]))
    return units


def src_to_ns(src: np.ndarray, unit: str, timebase: tuple[int, int] | None) -> np.ndarray:
    """Exact conversion; mach ticks use the same split as kf_time_ticks_to_ns_with()."""
    if unit == "mach ticks":
        assert timebase is not None
        numer, denom = timebase
        return (src // denom) * numer + ((src % denom) * numer) // denom
    return src * {"ns": 1, "us": 1000, "ms": MS}[unit]


def fit_clock(
    src: np.ndarray, arrival: np.ndarray, timebase: tuple[int, int] | None
) -> ClockFit | None:
    """Measures the units of a source clock by regressing arrival times against it."""
    if src.size < 2 or src[-1] <= src[0] or arrival[-1] <= arrival[0]:
        return None
    x = (src - src[0]).astype(np.float64)
    y = (arrival - arrival[0]).astype(np.float64)
    slope = float(np.polyfit(x, y, 1)[0])
    if not slope > 0.0:
        return ClockFit("unknown", slope, False, None)
    unit, nominal = min(_clock_units(timebase), key=lambda u: abs(math.log(slope / u[1])))
    if abs(slope / nominal - 1.0) > 0.01:
        return ClockFit("unknown", slope, False, None)
    delay = arrival - src_to_ns(src, unit, timebase)
    same = abs(float(np.median(delay))) < 1e9
    return ClockFit(unit, slope, same, summary(delay) if same else None)


def event_times(
    src: np.ndarray, arrival: np.ndarray, fit: ClockFit | None, timebase: tuple[int, int] | None
) -> tuple[np.ndarray, str]:
    """Source timestamps in ns when they share the arrival clock (more precise), else arrival."""
    if fit is not None and fit.same_clock:
        return src_to_ns(src, fit.unit, timebase), "source"
    return arrival, "arrival"


# ---------------------------------------------------------------------------- sample rate


@dataclass
class Rate:
    hz: float  # from the median interval
    effective_hz: float  # samples per second over the whole log
    median_dt_ns: float
    p5_dt_ns: float
    p95_dt_ns: float
    gaps: int  # intervals over 1.5x the median
    missing: int  # samples those gaps imply
    non_monotonic: int
    duration_s: float


def sample_rate(t_ns: np.ndarray) -> Rate | None:
    if t_ns.size < 3:
        return None
    dt = np.diff(t_ns).astype(np.float64)
    positive = dt[dt > 0]
    if not positive.size:
        return None
    median = float(np.median(positive))
    gap = positive > 1.5 * median
    missing = int(np.round(positive[gap] / median).sum()) - int(gap.sum())
    duration = float(t_ns[-1] - t_ns[0]) / 1e9
    p5, p95 = np.percentile(positive, [5, 95])
    return Rate(
        hz=1e9 / median,
        effective_hz=(t_ns.size - 1) / duration if duration > 0 else 0.0,
        median_dt_ns=median,
        p5_dt_ns=float(p5),
        p95_dt_ns=float(p95),
        gaps=int(gap.sum()),
        missing=missing,
        non_monotonic=int((dt <= 0).sum()),
        duration_s=duration,
    )


# ---------------------------------------------------------------------------- bytes


@dataclass
class ByteStat:
    offset: int
    minimum: int
    maximum: int
    distinct: int
    std: float


def byte_stats(reports: np.ndarray) -> list[ByteStat]:
    return [
        ByteStat(
            offset=i,
            minimum=int(column.min()),
            maximum=int(column.max()),
            distinct=int(np.unique(column).size),
            std=float(column.std()),
        )
        for i, column in enumerate(reports.T)
    ]


def _unsigned(reports: np.ndarray, offset: int, width: int) -> np.ndarray:
    raw = np.ascontiguousarray(reports[:, offset : offset + width])
    return raw.view({1: "<u1", 2: "<u2", 4: "<u4"}[width]).reshape(-1).astype(np.int64)


def find_counters(reports: np.ndarray) -> list[dict[str, Any]]:
    """Little-endian unsigned fields that increase on almost every report (counters, clocks)."""
    found: list[dict[str, Any]] = []
    covered: set[int] = set()
    n, length = reports.shape
    if n < 16:
        return found
    for width in (4, 2, 1):
        modulus = 1 << (8 * width)
        for offset in range(0, length - width + 1):
            span = set(range(offset, offset + width))
            if span & covered:
                continue
            values = _unsigned(reports, offset, width)
            step = np.diff(values) % modulus
            if np.mean((step > 0) & (step < modulus // 2)) >= 0.99 and (
                np.unique(values).size >= n // 2
            ):
                found.append(
                    {"offset": offset, "type": f"uint{8 * width}", "step": float(np.median(step))}
                )
                covered |= span
    return found


# ---------------------------------------------------------------------------- layouts


@dataclass(frozen=True)
class Layout:
    dtype: str
    offset: int

    @property
    def size(self) -> int:
        return np.dtype(DTYPES[self.dtype]).itemsize

    @property
    def is_float(self) -> bool:
        return self.dtype == "float32"

    def fits(self, length: int) -> bool:
        return self.offset + 3 * self.size <= length

    def decode(self, reports: np.ndarray) -> np.ndarray:
        """Raw axis values, shape (n, 3), as float64."""
        if not self.fits(reports.shape[1]):
            raise LogError(f"layout {self} needs {self.offset + 3 * self.size} bytes per report")
        raw = np.ascontiguousarray(reports[:, self.offset : self.offset + 3 * self.size])
        with np.errstate(invalid="ignore", over="ignore"):  # NaN floats are rejected later.
            return raw.view(DTYPES[self.dtype]).reshape(-1, 3).astype(np.float64)

    def __str__(self) -> str:
        return f"{self.dtype}x3@{self.offset}"

    @classmethod
    def parse(cls, text: str) -> Layout:
        match = re.fullmatch(r"(int16|int32|float32)x3@(\d+)", text)
        if not match:
            raise ValueError(f"bad layout {text!r}: expected TYPEx3@OFFSET, e.g. int32x3@6")
        return cls(match.group(1), int(match.group(2)))


def candidate_layouts(length: int) -> list[Layout]:
    return [Layout(d, o) for d in DTYPES for o in range(length) if Layout(d, o).fits(length)]


@dataclass
class Scale:
    g_per_unit: float  # makes the median magnitude 1 g (the log must be mostly at rest)
    nearest: str  # closest conventional factor, e.g. "2^-16", "1e-3" or "1/g (m/s^2)"
    nearest_g_per_unit: float
    error: float  # relative difference between the inferred and the nearest factor


def _conventional_scales() -> list[tuple[str, float]]:
    scales = [(f"2^-{k}", 2.0**-k) for k in range(0, 25)]
    scales += [(f"1e-{k}", 10.0**-k) for k in range(0, 7)]
    scales += [(f"1e-{k}/g (m/s^2)", 10.0**-k / STANDARD_GRAVITY) for k in range(0, 4)]
    return scales


def infer_scale(median_magnitude: float) -> Scale | None:
    if not median_magnitude > 0.0:
        return None
    g_per_unit = 1.0 / median_magnitude
    name, value = min(_conventional_scales(), key=lambda s: abs(math.log(g_per_unit / s[1])))
    return Scale(g_per_unit, name, value, g_per_unit / value - 1.0)


def trailing_deviation(t_ns: np.ndarray, xyz: np.ndarray, window_ns: int) -> np.ndarray:
    """|sample - mean of the samples in the preceding window_ns|, per sample (0 at the start)."""
    n = xyz.shape[0]
    out = np.zeros(n, dtype=np.float64)
    if n < 2:
        return out
    starts = np.searchsorted(t_ns, t_ns - window_ns, side="left")
    cumulative = np.vstack([np.zeros((1, 3)), np.cumsum(xyz, axis=0)])
    idx = np.arange(n)
    count = idx - starts
    valid = count > 0
    mean = (cumulative[idx[valid]] - cumulative[starts[valid]]) / count[valid, None]
    out[valid] = np.linalg.norm(xyz[valid] - mean, axis=1)
    return out


def key_response(
    t_ns: np.ndarray, deviation: np.ndarray, keys_ns: np.ndarray, span_ns: int = RESPONSE_NS
) -> float | None:
    """Median post-key peak deviation over the same statistic at times far from any key."""
    if keys_ns.size < 3 or t_ns.size < 100:
        return None

    def peak_after(start: int) -> float:
        lo, hi = np.searchsorted(t_ns, [start, start + span_ns])
        return float(deviation[lo:hi].max()) if hi > lo else 0.0

    key_peaks = [peak_after(int(k)) for k in keys_ns]
    grid = np.arange(t_ns[0] + 100 * MS, t_ns[-1] - span_ns, span_ns)
    near = np.searchsorted(keys_ns, grid)
    far = []
    for g, i in zip(grid, near):
        before = keys_ns[i - 1] if i > 0 else None
        after = keys_ns[i] if i < keys_ns.size else None
        if (before is None or g - before > 2 * ISOLATION_NS) and (
            after is None or after - g > ISOLATION_NS
        ):
            far.append(peak_after(int(g)))
    if len(far) < 10:
        return None
    quiet = float(np.median(far))
    return float(np.median(key_peaks)) / quiet if quiet > 0 else float("inf")


@dataclass
class LayoutScore:
    layout: Layout
    usable: bool
    reason: str  # why it was rejected
    magnitude: float  # median |a| in raw units
    roughness: float  # rms first difference / median |a|: noise relative to gravity
    lsb_live: bool  # every axis's least significant byte varies (always true for floats)
    key_snr: float | None
    scale: Scale | None
    shifted_copy_of: str | None = None  # same signal as a better-ranked layout


def score_layout(
    layout: Layout,
    reports: np.ndarray,
    t_ns: np.ndarray | None = None,
    keys_ns: np.ndarray | None = None,
) -> LayoutScore:
    def rejected(reason: str) -> LayoutScore:
        return LayoutScore(layout, False, reason, 0.0, math.inf, False, None, None)

    values = layout.decode(reports)
    n = values.shape[0]
    if n < 8:
        return rejected("too few reports")
    if not np.isfinite(values).all() or np.abs(values).max() > 1e9:
        return rejected("non-finite or implausible values")
    needed = min(16, n // 2)
    if min(np.unique(values[:, i]).size for i in range(3)) < needed:
        return rejected("an axis is constant or nearly so")
    diffs = np.diff(values, axis=0)
    if any(np.mean(diffs[:, i] > 0) > 0.95 for i in range(3)):
        return rejected("an axis only increases (counter or clock)")
    magnitude = np.linalg.norm(values, axis=1)
    median = float(np.median(magnitude))
    if not median > 0.0:
        return rejected("zero magnitude")
    roughness = float(np.sqrt(np.mean(diffs**2))) / median
    lsb_live = layout.is_float or all(
        np.unique(reports[:, layout.offset + i * layout.size]).size >= min(8, n // 4)
        for i in range(3)
    )
    snr = None
    if t_ns is not None and keys_ns is not None and keys_ns.size:
        deviation = trailing_deviation(t_ns, values / median, 50 * MS)
        snr = key_response(t_ns, deviation, keys_ns)
    return LayoutScore(layout, True, "", median, roughness, lsb_live, snr, infer_scale(median))


def rank_layouts(
    reports: np.ndarray,
    t_ns: np.ndarray | None = None,
    keys_ns: np.ndarray | None = None,
) -> list[LayoutScore]:
    """Every candidate, best first. Usable before rejected; live LSBs first; then the strongest
    key response; then the smoothest signal relative to gravity."""
    scores = [score_layout(c, reports, t_ns, keys_ns) for c in candidate_layouts(reports.shape[1])]
    scores.sort(
        key=lambda s: (not s.usable, not s.lsb_live, -(s.key_snr or 0.0), s.roughness)
    )
    # Flag byte-shifted views of one field (for example a 24-bit value read one byte early):
    # their signals are identical up to a factor, so only the scale tells them apart.
    usable = [s for s in scores if s.usable][:12]
    decoded = {s.layout: s.layout.decode(reports) for s in usable}
    for i, later in enumerate(usable):
        for earlier in usable[:i]:
            a, b = decoded[earlier.layout], decoded[later.layout]
            with np.errstate(invalid="ignore", divide="ignore"):
                corr = [abs(np.corrcoef(a[:, k], b[:, k])[0, 1]) for k in range(3)]
            if all(c > 0.999 for c in corr):
                later.shifted_copy_of = str(earlier.layout)
                break
    return scores


def sorted_rows(log: SensorLog) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """(reports, report times, key-down times), time-ordered, on the best available clock."""
    timebase = log.timebase
    report_fit = fit_clock(log.report_src, log.report_arrival_ns, timebase)
    key_fit = fit_clock(log.keydown_src, log.keydown_arrival_ns, timebase)
    t_ns, report_base = event_times(log.report_src, log.report_arrival_ns, report_fit, timebase)
    keys, key_base = event_times(log.keydown_src, log.keydown_arrival_ns, key_fit, timebase)
    if report_base != key_base:
        t_ns, keys = log.report_arrival_ns, log.keydown_arrival_ns
    order = np.argsort(t_ns, kind="stable")
    return log.reports[order], t_ns[order], np.sort(keys)


# ---------------------------------------------------------------------------- signals


@dataclass
class Signal:
    log: SensorLog
    layout: Layout
    g_per_unit: float
    t_ns: np.ndarray  # sample times, ns on the kf_time clock
    xyz: np.ndarray  # g
    report_clock: ClockFit | None
    key_clock: ClockFit | None
    time_base: str  # "source" or "arrival": what t_ns and the key times are based on
    keydown_ns: np.ndarray
    keyup_ns: np.ndarray
    rate: Rate | None


def build_signal(log: SensorLog, layout: Layout, g_per_unit: float) -> Signal:
    timebase = log.timebase
    report_clock = fit_clock(log.report_src, log.report_arrival_ns, timebase)
    key_clock = fit_clock(log.keydown_src, log.keydown_arrival_ns, timebase)
    t_ns, report_base = event_times(log.report_src, log.report_arrival_ns, report_clock, timebase)
    keydown, key_base = event_times(log.keydown_src, log.keydown_arrival_ns, key_clock, timebase)
    keyup, _ = event_times(log.keyup_src, log.keyup_arrival_ns, key_clock, timebase)
    if report_base != key_base:
        # Mixing bases would skew every delay by the delivery latency: use arrival for both.
        t_ns, keydown, keyup = log.report_arrival_ns, log.keydown_arrival_ns, log.keyup_arrival_ns
        report_base = "arrival"
    order = np.argsort(t_ns, kind="stable")
    xyz = layout.decode(log.reports)[order] * g_per_unit
    t_sorted = t_ns[order]
    return Signal(
        log=log,
        layout=layout,
        g_per_unit=g_per_unit,
        t_ns=t_sorted,
        xyz=xyz,
        report_clock=report_clock,
        key_clock=key_clock,
        time_base=report_base,
        keydown_ns=np.sort(keydown),
        keyup_ns=np.sort(keyup),
        rate=sample_rate(t_sorted),
    )


def window_peak(
    t_ns: np.ndarray,
    xyz: np.ndarray,
    t_key: int,
    pre_ns: int,
    post_ns: int,
    baseline_ns: int,
    min_samples: int,
) -> tuple[float, int, int] | None:
    """kf_velocity_peak_g() without row gain: (peak g, peak time, window samples) or None."""
    window_start = t_key - pre_ns if t_key > pre_ns else 0
    window_end = t_key + post_ns
    baseline_start = window_start - baseline_ns if window_start > baseline_ns else 0
    b0 = int(np.searchsorted(t_ns, baseline_start, side="left"))
    b1 = int(np.searchsorted(t_ns, window_start, side="left"))
    w1 = int(np.searchsorted(t_ns, window_end, side="right"))
    if b1 == b0 or (w1 - b1) * 2 < min_samples or w1 == b1:
        return None
    base = xyz[b0:b1].mean(axis=0)
    deviation = np.linalg.norm(xyz[b1:w1] - base, axis=1)
    k = int(np.argmax(deviation))
    return float(deviation[k]), int(t_ns[b1 + k]), w1 - b1


def expected_samples(pre_ns: int, post_ns: int, rate_hz: float) -> int:
    return int((pre_ns + post_ns) * rate_hz / 1e9)


# ---------------------------------------------------------------------------- keystrokes


@dataclass
class Keystroke:
    log: str
    tags: dict[str, str]
    t_ns: int
    isolated: bool
    delay_ns: float | None = None  # key-down -> largest deviation in the search window
    onset_ns: float | None = None  # key-down -> first sample clearly above the baseline
    search_peak_g: float | None = None
    peak_g: float | None = None  # the core's estimator with the evaluated windows
    peak_g_config: float | None = None  # the core's estimator with kf_config.h's windows


def analyze_keystrokes(
    signal: Signal,
    pre_ns: int,
    post_ns: int,
    baseline_ns: int,
    config: dict[str, int],
) -> list[Keystroke]:
    rate = signal.rate.hz if signal.rate else float(config["KF_SENSOR_NOMINAL_HZ"])
    keys = signal.keydown_ns
    out: list[Keystroke] = []
    for i, t_key in enumerate(keys):
        t_key = int(t_key)
        isolated = (i == 0 or t_key - int(keys[i - 1]) > ISOLATION_NS) and (
            i + 1 == keys.size or int(keys[i + 1]) - t_key > SEARCH_POST_NS
        )
        k = Keystroke(signal.log.name, signal.log.tags, t_key, isolated)
        search = window_peak(
            signal.t_ns,
            signal.xyz,
            t_key,
            SEARCH_PRE_NS,
            SEARCH_POST_NS,
            baseline_ns,
            expected_samples(SEARCH_PRE_NS, SEARCH_POST_NS, rate),
        )
        if search is not None:
            k.search_peak_g, t_peak, _ = search
            k.delay_ns = float(t_peak - t_key)
            k.onset_ns = _onset(signal, t_key, baseline_ns, k.search_peak_g)
        for attr, pre, post in (
            ("peak_g", pre_ns, post_ns),
            ("peak_g_config", config["KF_WINDOW_PRE_NS"], config["KF_WINDOW_POST_NS"]),
        ):
            peak = window_peak(
                signal.t_ns,
                signal.xyz,
                t_key,
                pre,
                post,
                baseline_ns,
                expected_samples(pre, post, rate),
            )
            setattr(k, attr, peak[0] if peak else None)
        out.append(k)
    return out


def _onset(signal: Signal, t_key: int, baseline_ns: int, peak_g: float) -> float | None:
    """First sample in the search window above twice the baseline's own largest deviation."""
    start = t_key - SEARCH_PRE_NS
    b0, b1, w1 = np.searchsorted(
        signal.t_ns, [start - baseline_ns, start, t_key + SEARCH_POST_NS], side="left"
    )
    if b1 - b0 < 2 or w1 <= b1:
        return None
    base = signal.xyz[b0:b1].mean(axis=0)
    rest = float(np.linalg.norm(signal.xyz[b0:b1] - base, axis=1).max())
    threshold = 2.0 * rest
    if not peak_g > threshold:
        return None  # No clear response above the noise.
    deviation = np.linalg.norm(signal.xyz[b1:w1] - base, axis=1)
    first = int(np.argmax(deviation > threshold))
    return float(signal.t_ns[b1 + first] - t_key)


def rest_peaks(signal: Signal, pre_ns: int, post_ns: int, baseline_ns: int) -> list[float]:
    """The estimator at a grid of pseudo-keystrokes: the noise floor the core would see."""
    if signal.t_ns.size < 10 or signal.rate is None:
        return []
    need = expected_samples(pre_ns, post_ns, signal.rate.hz)
    first = int(signal.t_ns[0]) + pre_ns + baseline_ns
    last = int(signal.t_ns[-1]) - post_ns
    peaks = []
    # One window apart, so the windows tile and no bump at rest falls between them.
    for t in range(first, last, pre_ns + post_ns):
        result = window_peak(signal.t_ns, signal.xyz, t, pre_ns, post_ns, baseline_ns, need)
        if result is not None:
            peaks.append(result[0])
    return peaks


# ---------------------------------------------------------------------------- report


def resolve_layout(
    logs: Sequence[SensorLog], layout_text: str | None, scale: float | None
) -> tuple[Layout, float, str]:
    """One layout and scale for every log, so their peaks are comparable."""
    with_reports = [log for log in logs if log.reports.size]
    if not with_reports:
        raise LogError("no log holds any reports")
    if layout_text:
        layout = Layout.parse(layout_text)
        source = "given"
    else:
        reference = max(with_reports, key=lambda log: log.reports.shape[0])
        reports, t_ns, keys = sorted_rows(reference)
        ranked = rank_layouts(reports, t_ns, keys if keys.size else None)
        best = next((s for s in ranked if s.usable), None)
        if best is None:
            raise LogError(f"{reference.name}: no candidate layout decodes plausibly")
        layout = best.layout
        source = f"auto from {reference.name}"
    if scale is not None:
        return layout, scale, source
    rest = [log for log in with_reports if log.tags.get("force") == "rest"] or with_reports
    magnitudes = np.concatenate(
        [np.linalg.norm(layout.decode(log.reports), axis=1) for log in rest]
    )
    inferred = infer_scale(float(np.median(magnitudes)))
    if inferred is None:
        raise LogError(f"layout {layout} decodes to zero magnitude")
    if abs(inferred.error) < 0.02:
        return layout, inferred.nearest_g_per_unit, f"{source}; scale {inferred.nearest} (snapped)"
    return layout, inferred.g_per_unit, f"{source}; scale inferred from 1 g at rest"


def _group(keystrokes: Sequence[Keystroke], **want: str) -> list[float]:
    return [
        k.peak_g
        for k in keystrokes
        if k.peak_g is not None and all(k.tags.get(t) == v for t, v in want.items())
    ]


def evaluate_gate(keystrokes: Sequence[Keystroke]) -> list[dict[str, Any]]:
    """Per surface and row: does the hard-tap p10 clear the soft-tap p90?"""
    rows: list[dict[str, Any]] = []
    surfaces = sorted({k.tags.get("surface", "") for k in keystrokes})
    names = {k.tags.get("row", "") for k in keystrokes} - {""}
    ordered = [r for r in ROW_ORDER if r in names] + sorted(names - set(ROW_ORDER))
    for surface in surfaces:
        for row in ordered:
            soft = _group(keystrokes, surface=surface, row=row, force="soft")
            hard = _group(keystrokes, surface=surface, row=row, force="hard")
            if not soft and not hard:
                continue
            entry: dict[str, Any] = {
                "surface": surface or "-",
                "row": row,
                "soft": summary(soft),
                "hard": summary(hard),
                "separates": None,
            }
            if len(soft) >= MIN_GROUP and len(hard) >= MIN_GROUP:
                entry["separates"] = bool(pct(hard, 10) > pct(soft, 90))  # type: ignore[operator]
            rows.append(entry)
    return rows


def recommend_windows(keystrokes: Sequence[Keystroke], rate_hz: float) -> dict[str, Any] | None:
    responsive = [k for k in keystrokes if k.isolated and k.onset_ns is not None]
    if len(responsive) < MIN_GROUP:
        return None
    period = 1e9 / rate_hz
    delays = [k.delay_ns for k in responsive if k.delay_ns is not None]
    onsets = [k.onset_ns for k in responsive if k.onset_ns is not None]
    delay, onset = summary(delays), summary(onsets)
    assert delay is not None and onset is not None
    post = ceil_ms(delay["p95"] + period)
    pre = max(MS, ceil_ms(max(0.0, -onset["p5"]) + period))
    return {
        "keystrokes": len(responsive),
        "delay_ns": delay,
        "onset_ns": onset,
        "pre_ns": pre,
        "post_ns": post,
        "post_within_budget": post <= POST_BUDGET_NS,
    }


def permissions(logs: Sequence[SensorLog]) -> list[dict[str, Any]]:
    return [
        {
            "log": log.name,
            "euid": log.meta.get("euid", "?"),
            "input_monitoring": log.meta.get("keys.listen_access", "?"),
            "tap": log.meta.get("keys.tap", "?"),
            "open": log.meta.get("device.open", "?"),
            "reports": int(log.reports.shape[0]) + log.other_length_reports,
            "keydowns": int(log.keydown_arrival_ns.size),
        }
        for log in logs
    ]


def needs_root(perms: Sequence[dict[str, Any]]) -> str:
    user_ok = any(p["euid"] != "0" and p["reports"] > 0 for p in perms)
    root_ok = any(p["euid"] == "0" and p["reports"] > 0 for p in perms)
    if user_ok:
        return "no"
    if root_ok:
        return "yes"
    return "unknown (no log has reports)"


def build_report(
    paths: Sequence[Path],
    layout_text: str | None = None,
    scale: float | None = None,
    pre_ms: float | None = None,
    post_ms: float | None = None,
) -> dict[str, Any]:
    config = read_config()
    logs = [read_log(Path(p)) for p in paths]
    baseline = config["KF_BASELINE_NS"]
    layout: Layout | None = None
    g_per_unit = 0.0
    layout_source = "no log holds any reports"
    signals: list[Signal] = []
    if any(log.reports.size for log in logs):
        layout, g_per_unit, layout_source = resolve_layout(logs, layout_text, scale)
        signals = [build_signal(log, layout, g_per_unit) for log in logs if log.reports.size]
    rates = [s.rate.hz for s in signals if s.rate]
    rate_hz = float(np.median(rates)) if rates else float(config["KF_SENSOR_NOMINAL_HZ"])

    # Pass 1 finds the timing; pass 2 measures peaks with the windows it recommends.
    tap_signals = [s for s in signals if s.log.tags.get("force") in ("soft", "hard", "typing")]
    discovery = [
        k
        for s in tap_signals
        for k in analyze_keystrokes(
            s, config["KF_WINDOW_PRE_NS"], config["KF_WINDOW_POST_NS"], baseline, config
        )
    ]
    windows = recommend_windows(discovery, rate_hz)
    pre = int(pre_ms * MS) if pre_ms is not None else (windows or {}).get("pre_ns")
    post = int(post_ms * MS) if post_ms is not None else (windows or {}).get("post_ns")
    pre = pre if pre is not None else config["KF_WINDOW_PRE_NS"]
    post = post if post is not None else config["KF_WINDOW_POST_NS"]
    keystrokes = [k for s in tap_signals for k in analyze_keystrokes(s, pre, post, baseline, config)]

    noise: dict[str, Any] = {}
    for s in signals:
        if s.log.tags.get("force") == "rest":
            surface = s.log.tags.get("surface", "-")
            noise.setdefault(surface, []).extend(rest_peaks(s, pre, post, baseline))
    perms = permissions(logs)
    gate = evaluate_gate(keystrokes)

    machines = sorted(
        {
            (log.meta.get("hw_model", "?"), log.meta.get("os", "?"),
             log.meta.get("started_utc", "?")[:10])
            for log in logs
        }
    )
    return {
        "config": config,
        "machines": [{"hw_model": m, "os": o, "date": d} for m, o, d in machines],
        "layout": str(layout) if layout is not None else None,
        "g_per_unit": g_per_unit,
        "layout_source": layout_source,
        "rate": {
            "median_hz": rate_hz,
            "per_log": {s.log.name: asdict(s.rate) if s.rate else None for s in signals},
        },
        "clocks": {
            s.log.name: {
                "reports": asdict(s.report_clock) if s.report_clock else None,
                "keys": asdict(s.key_clock) if s.key_clock else None,
                "time_base": s.time_base,
            }
            for s in signals
        },
        "windows": windows,
        "evaluated_pre_ns": pre,
        "evaluated_post_ns": post,
        "gate": gate,
        "noise_floor_g": {surface: summary(p) for surface, p in noise.items()},
        "permissions": perms,
        "needs_root": needs_root(perms),
        "keystrokes": [asdict(k) for k in keystrokes],
        "verdict": verdict(gate, windows, needs_root(perms)),
    }


def verdict(
    gate: Sequence[dict[str, Any]], windows: dict[str, Any] | None, root: str
) -> dict[str, Any]:
    """The gate's three criteria as measured. The decision itself is the owner's."""
    checks = {}
    for row in GATE_ROWS:
        results = [g["separates"] for g in gate if g["row"] == row and g["separates"] is not None]
        checks[f"{row}_separates"] = all(results) if results else None
    checks["works_without_root"] = {"no": True, "yes": False}.get(root)
    checks["post_within_10ms"] = windows["post_within_budget"] if windows else None
    known = [v for v in checks.values() if v is not None]
    if len(known) < len(checks):
        suggestion = "incomplete: record the full protocol"
    elif all(known):
        suggestion = "go"
    elif checks["home_separates"] or checks["space_separates"]:
        suggestion = "go with limits"
    else:
        suggestion = "no-go"
    return {"checks": checks, "suggestion": suggestion}


# ---------------------------------------------------------------------------- formatting


def _ms(ns: float | None) -> str:
    return "-" if ns is None else f"{ns / MS:.2f}"


def _g(value: float | None) -> str:
    return "-" if value is None else f"{value:.4f}"


def _yes(value: bool | None) -> str:
    return "-" if value is None else ("yes" if value else "no")


def format_markdown(r: dict[str, Any]) -> str:
    out: list[str] = []
    add = out.append
    add("## Test machines\n")
    add("| Mac model (`hw.model`) | macOS | Date |")
    add("| --- | --- | --- |")
    for m in r["machines"]:
        add(f"| {m['hw_model']} | {m['os']} | {m['date']} |")
    add("\n## Reports\n")
    if r["layout"] is None:
        add(f"- Layout: - ({r['layout_source']})")
    else:
        add(f"- Layout: `{r['layout']}` ({r['layout_source']})")
        add(f"- Scale: {r['g_per_unit']:.9g} g per unit")
    add(f"- Measured sample rate (median over logs): {r['rate']['median_hz']:.1f} Hz")
    units = sorted(
        {
            f"{kind}: {c[kind]['unit']}" + (" (same clock as mach_absolute_time)" if c[kind]["same_clock"] else "")
            for c in r["clocks"].values()
            for kind in ("reports", "keys")
            if c[kind]
        }
    )
    add(f"- Timestamp units: {'; '.join(units) if units else '-'}")
    add("\n## Timing\n")
    add("| Measurement | Median | p95 |")
    add("| --- | --- | --- |")
    w = r["windows"]
    if w:
        add(f"| Key-down event → force peak (ms) | {_ms(w['delay_ns']['p50'])} | {_ms(w['delay_ns']['p95'])} |")
        add(f"| Key-down event → response onset (ms) | {_ms(w['onset_ns']['p50'])} | {_ms(w['onset_ns']['p95'])} |")
        add(f"| Resulting `KF_WINDOW_PRE_NS` / `KF_WINDOW_POST_NS` | {_ms(w['pre_ns'])} / {_ms(w['post_ns'])} ms | |")
    else:
        add("| Key-down event → force peak (ms) | not enough responsive taps | |")
    add("\n## Separability\n")
    add(f"Peaks use PRE {_ms(r['evaluated_pre_ns'])} ms and POST {_ms(r['evaluated_post_ns'])} ms.\n")
    add("| Surface | Row | Soft n | Soft p90 (g) | Hard n | Hard p10 (g) | Separates? |")
    add("| --- | --- | --- | --- | --- | --- | --- |")
    for g in r["gate"]:
        soft, hard = g["soft"] or {}, g["hard"] or {}
        add(
            f"| {g['surface']} | {g['row']} | {soft.get('n', 0)} | {_g(soft.get('p90'))} | "
            f"{hard.get('n', 0)} | {_g(hard.get('p10'))} | {_yes(g['separates'])} |"
        )
    add("\n## Noise floor\n")
    add("| Setting | Peak at rest p50 (g) | p95 (g) | max (g) |")
    add("| --- | --- | --- | --- |")
    for surface, s in sorted(r["noise_floor_g"].items()):
        s = s or {}
        add(f"| {surface} | {_g(s.get('p50'))} | {_g(s.get('p95'))} | {_g(s.get('max'))} |")
    add("\n## Permissions\n")
    add("| Log | euid | Input Monitoring | Tap | IOHIDDeviceOpen | Reports |")
    add("| --- | --- | --- | --- | --- | --- |")
    for p in r["permissions"]:
        add(f"| {p['log']} | {p['euid']} | {p['input_monitoring']} | {p['tap']} | {p['open']} | {p['reports']} |")
    add(f"\nNeeds root: {r['needs_root']}.")
    v = r["verdict"]
    add("\n## Gate checks (measured; the decision is the owner's)\n")
    for name, value in v["checks"].items():
        add(f"- {name.replace('_', ' ')}: {_yes(value)}")
    add(f"- Suggests: **{v['suggestion']}**")
    return "\n".join(out) + "\n"


def format_keystrokes(r: dict[str, Any]) -> str:
    lines = ["\n## Keystrokes\n", "| Log | t (s) | Isolated | Delay (ms) | Onset (ms) | Peak (g) |",
             "| --- | --- | --- | --- | --- | --- |"]
    starts: dict[str, int] = {}
    for k in r["keystrokes"]:
        t0 = starts.setdefault(k["log"], k["t_ns"])
        lines.append(
            f"| {k['log']} | {(k['t_ns'] - t0) / 1e9:.3f} | {_yes(k['isolated'])} | "
            f"{_ms(k['delay_ns'])} | {_ms(k['onset_ns'])} | {_g(k['peak_g'])} |"
        )
    return "\n".join(lines) + "\n"


def print_inspect(log: SensorLog, top: int) -> None:
    print(f"{log.name}: {log.meta.get('hw_model', '?')}, macOS {log.meta.get('os', '?')}")
    print(f"  label        {log.meta.get('label', '') or '-'}")
    print(
        f"  device       {log.meta.get('device.product', '?')} "
        f"({log.meta.get('device.transport', '?')}), open {log.meta.get('device.open', '?')}, "
        f"euid {log.meta.get('euid', '?')}, Input Monitoring {log.meta.get('keys.listen_access', '?')}"
    )
    print(
        f"  rows         {log.reports.shape[0]} reports of {log.report_length} bytes"
        f" (+{log.other_length_reports} other lengths), {log.keydown_arrival_ns.size} key-downs,"
        f" {log.keyup_arrival_ns.size} key-ups"
    )
    if not log.reports.size:
        print("  no reports: nothing to decode")
        return
    timebase = log.timebase
    for what, src, arrival in (
        ("report clock", log.report_src, log.report_arrival_ns),
        ("key clock   ", log.keydown_src, log.keydown_arrival_ns),
    ):
        fit = fit_clock(src, arrival, timebase)
        if fit is None:
            print(f"  {what} -")
            continue
        line = f"  {what} {fit.unit}, {fit.ns_per_unit:.4f} ns per unit"
        if fit.delay_ns:
            line += (
                f", same clock; delivery delay p50 {fit.delay_ns['p50'] / 1e3:.1f} us,"
                f" p95 {fit.delay_ns['p95'] / 1e3:.1f} us"
            )
        print(line)
    reports, t_ns, keys = sorted_rows(log)
    rate = sample_rate(t_ns)
    if rate:
        print(
            f"  rate         {rate.hz:.1f} Hz (median interval {rate.median_dt_ns / 1e3:.1f} us,"
            f" p5 {rate.p5_dt_ns / 1e3:.1f}, p95 {rate.p95_dt_ns / 1e3:.1f}),"
            f" {rate.gaps} gaps ~{rate.missing} missing, {rate.non_monotonic} out of order"
        )
    print("\n  byte  min  max  distinct     std")
    for b in byte_stats(log.reports):
        flag = "  constant" if b.distinct == 1 else ""
        print(f"  {b.offset:4d} {b.minimum:4d} {b.maximum:4d} {b.distinct:9d} {b.std:7.2f}{flag}")
    counters = find_counters(log.reports)
    for c in counters:
        print(f"  counter      {c['type']} at byte {c['offset']}, median step {c['step']:g}")
    ranked = rank_layouts(reports, t_ns, keys if keys.size else None)
    print(f"\n  {'layout':<14} {'roughness':>10} {'|a| raw':>12} {'key SNR':>8}  scale")
    for s in [s for s in ranked if s.usable][:top]:
        scale = (
            f"{s.scale.nearest} ({s.scale.error:+.1%})" if s.scale is not None else "-"
        )
        note = f"  shifted copy of {s.shifted_copy_of}" if s.shifted_copy_of else ""
        note += "" if s.lsb_live else "  LSB constant"
        snr = "-" if s.key_snr is None else f"{s.key_snr:.2f}"
        print(f"  {str(s.layout):<14} {s.roughness:10.5f} {s.magnitude:12.5g} {snr:>8}  {scale}{note}")
    rejected = sum(1 for s in ranked if not s.usable)
    print(f"  ({rejected} candidates rejected)")


# ---------------------------------------------------------------------------- plot

# Reference palette (dataviz skill): series slot 1 for the signal, slot 2 for key-downs,
# secondary ink for key-ups, text tokens for every label, a recessive grid.
PLOT_SURFACE = "#fcfcfb"
PLOT_TEXT = "#0b0b0b"
PLOT_TEXT_SECONDARY = "#52514e"
PLOT_GRID = "#e4e3df"
PLOT_SIGNAL = "#2a78d6"
PLOT_KEYDOWN = "#eb6834"


def plot_log(signal: Signal, out: Path) -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.lines import Line2D

    t0 = int(signal.t_ns[0])
    seconds = (signal.t_ns - t0) / 1e9
    deviation = trailing_deviation(signal.t_ns, signal.xyz, 50 * MS)
    fig, ax = plt.subplots(figsize=(14, 4.2), dpi=150)
    fig.patch.set_facecolor(PLOT_SURFACE)
    ax.set_facecolor(PLOT_SURFACE)
    for t in signal.keyup_ns:
        ax.axvline((t - t0) / 1e9, color=PLOT_TEXT_SECONDARY, lw=0.7, ls=(0, (2, 2)), alpha=0.7)
    for t in signal.keydown_ns:
        ax.axvline((t - t0) / 1e9, color=PLOT_KEYDOWN, lw=1.0, alpha=0.9)
    ax.plot(seconds, deviation, color=PLOT_SIGNAL, lw=1.0, solid_capstyle="round")
    handles = [
        Line2D([], [], color=PLOT_SIGNAL, lw=1.6, label="|a − 50 ms trailing mean|"),
        Line2D([], [], color=PLOT_KEYDOWN, lw=1.6, label="key-down"),
        Line2D([], [], color=PLOT_TEXT_SECONDARY, lw=1.2, ls=(0, (2, 2)), label="key-up"),
    ]
    ax.legend(handles=handles, loc="upper right", frameon=False, labelcolor=PLOT_TEXT_SECONDARY)
    ax.grid(axis="y", color=PLOT_GRID, lw=0.6)
    ax.set_axisbelow(True)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        ax.spines[side].set_color(PLOT_GRID)
    ax.tick_params(colors=PLOT_TEXT_SECONDARY, labelsize=8)
    ax.set_xlabel("time (s)", color=PLOT_TEXT_SECONDARY, fontsize=9)
    ax.set_ylabel("deviation (g)", color=PLOT_TEXT_SECONDARY, fontsize=9)
    ax.set_xlim(0, float(seconds[-1]) if seconds.size else 1.0)
    ax.set_ylim(bottom=0)
    label = signal.log.meta.get("label", "")
    title = f"{signal.log.name}" + (f"  ·  {label}" if label else "")
    ax.set_title(
        f"{title}\n{signal.layout}, {signal.rate.hz:.0f} Hz" if signal.rate else title,
        loc="left", color=PLOT_TEXT, fontsize=10,
    )
    fig.tight_layout()
    fig.savefig(out, facecolor=PLOT_SURFACE)
    plt.close(fig)


# ---------------------------------------------------------------------------- fixture


def write_fixture(source: Path, out: Path, max_bytes: int) -> int:
    """Copies the header and the first rows of `source`, verbatim, up to max_bytes."""
    read_log(source)  # Validate before copying anything.
    written = 0
    rows = 0
    lines: list[str] = []
    with source.open(encoding="utf-8") as handle:
        in_rows = False
        for line in handle:
            if not in_rows:
                lines.append(line)
                written += len(line)
                in_rows = line.rstrip("\r\n") == COLUMNS
                continue
            if line.startswith("#"):
                continue
            if written + len(line) > max_bytes - 128:
                break
            lines.append(line)
            written += len(line)
            rows += 1
    lines.append(f"# fixture.source={source.name}\n# fixture.rows={rows}\n")
    out.write_text("".join(lines), encoding="utf-8")
    return rows


# ---------------------------------------------------------------------------- CLI


def _layout_and_scale(args: argparse.Namespace, logs: Sequence[SensorLog]) -> tuple[Layout, float]:
    layout, g_per_unit, _ = resolve_layout(logs, args.layout, args.scale)
    return layout, g_per_unit


def _jsonable(value: Any) -> Any:
    if isinstance(value, dict):
        return {str(k): _jsonable(v) for k, v in value.items()}
    if isinstance(value, (list, tuple)):
        return [_jsonable(v) for v in value]
    if isinstance(value, float) and not math.isfinite(value):
        return None
    if isinstance(value, np.generic):
        return value.item()
    return value


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="analyze.py",
        description=__doc__.splitlines()[0],
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="\n".join(__doc__.splitlines()[2:]),
    )
    sub = parser.add_subparsers(dest="command", required=True)

    def decode_flags(p: argparse.ArgumentParser) -> None:
        p.add_argument("--layout", help="TYPEx3@OFFSET; default: best candidate")
        p.add_argument("--scale", type=float, help="g per raw unit; default: from 1 g at rest")

    inspect = sub.add_parser("inspect", help="byte statistics and candidate layouts")
    inspect.add_argument("log", type=Path)
    inspect.add_argument("--top", type=int, default=8, help="candidates to show")

    report = sub.add_parser("report", help="keystroke statistics and the gate")
    report.add_argument("logs", type=Path, nargs="+")
    decode_flags(report)
    report.add_argument("--pre-ms", type=float, help="PRE window; default: recommended")
    report.add_argument("--post-ms", type=float, help="POST window; default: recommended")
    report.add_argument("--keys", action="store_true", help="also list every keystroke")
    report.add_argument("--json", action="store_true", help="machine-readable output")

    plot = sub.add_parser("plot", help="deviation over time with key markers")
    plot.add_argument("log", type=Path)
    plot.add_argument("--out", type=Path, required=True)
    decode_flags(plot)

    fixture = sub.add_parser("fixture", help="small excerpt for Tests/Fixtures/sensor/")
    fixture.add_argument("log", type=Path)
    fixture.add_argument("--out", type=Path, required=True)
    fixture.add_argument("--max-bytes", type=int, default=100_000)

    args = parser.parse_args(argv)
    try:
        if args.command == "inspect":
            print_inspect(read_log(args.log), args.top)
        elif args.command == "report":
            result = build_report(args.logs, args.layout, args.scale, args.pre_ms, args.post_ms)
            if args.json:
                json.dump(_jsonable(result), sys.stdout, indent=2)
                print()
            else:
                print(format_markdown(result), end="")
                if args.keys:
                    print(format_keystrokes(result), end="")
        elif args.command == "plot":
            log = read_log(args.log)
            layout, g_per_unit = _layout_and_scale(args, [log])
            plot_log(build_signal(log, layout, g_per_unit), args.out)
            print(f"wrote {args.out}")
        elif args.command == "fixture":
            rows = write_fixture(args.log, args.out, args.max_bytes)
            print(f"wrote {args.out}: {rows} rows")
    except (LogError, ValueError) as exc:
        print(f"analyze.py: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
