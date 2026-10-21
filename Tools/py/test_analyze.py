# SPDX-License-Identifier: LicenseRef-Proprietary
# Copyright (c) 2026 The Keyfeel Authors. All rights reserved.
"""Unit tests for analyze.py, run by `make test-py`.

Every test runs on synthetic logs with known ground truth. The synthetic report layout
(a uint32 counter, two constant bytes, three int32 axes at byte 6, 65536 counts per g) is an
arbitrary test choice, not a claim about the real sensor: Milestone 0 measures that.
"""
from __future__ import annotations

import contextlib
import io
import json
import tempfile
import unittest
from pathlib import Path

import numpy as np

import analyze as az

MS = az.MS
TIMEBASE = (125, 3)  # Apple Silicon: 24 MHz ticks.
NOISE_G = 0.0005
COUNTS_PER_G = 65536
DELAY_MS = 4.0
T0_NS = 5_000_000_000_000
TAP_SPACING_NS = 300 * MS


def ticks(ns: np.ndarray | int) -> np.ndarray | int:
    return ns * TIMEBASE[1] // TIMEBASE[0]


def pulse(rel_ms: np.ndarray) -> np.ndarray:
    """Rise over 1 ms, flat for 2 ms around the nominal peak, decay over 3 ms."""
    out = np.zeros_like(rel_ms)
    rising = (rel_ms >= -2.0) & (rel_ms < -1.0)
    out[rising] = rel_ms[rising] + 2.0
    out[(rel_ms >= -1.0) & (rel_ms <= 1.0)] = 1.0
    falling = (rel_ms > 1.0) & (rel_ms <= 4.0)
    out[falling] = (4.0 - rel_ms[falling]) / 3.0
    return out


def synth_log(
    path: Path,
    label: str,
    *,
    taps: list[float] = (),  # type: ignore[assignment]
    seconds: float = 2.0,
    rate_hz: float = 1000.0,
    drop_every: int = 0,
    seed: int = 0,
    euid: int = 501,
    key_src_scale: float | None = None,  # None: CGEvent timestamps in mach ticks
    report_src_unit: str = "ticks",
    with_reports: bool = True,
) -> Path:
    rng = np.random.default_rng(seed)
    seconds = max(seconds, (len(taps) + 2) * TAP_SPACING_NS / 1e9)
    n = int(seconds * rate_hz)
    period = 1e9 / rate_hz
    t_true = (T0_NS + np.arange(n) * period + rng.uniform(-20e3, 20e3, n)).astype(np.int64)
    keep = np.ones(n, dtype=bool)
    if drop_every:
        keep[drop_every::drop_every] = False

    gravity = np.array([0.02, -0.05, 0.9985])
    gravity /= np.linalg.norm(gravity)
    direction = np.array([0.3, 0.1, 1.0])
    direction /= np.linalg.norm(direction)
    xyz = gravity + rng.normal(0.0, NOISE_G, (n, 3))
    key_times = []
    for i, amp in enumerate(taps):
        t_key = T0_NS + TAP_SPACING_NS * (i + 1)
        key_times.append(t_key)
        rel = (t_true - (t_key + DELAY_MS * MS)) / MS
        xyz += amp * pulse(rel)[:, None] * direction

    counts = np.round(xyz * COUNTS_PER_G).astype("<i4")
    reports = np.zeros((n, 22), dtype=np.uint8)
    reports[:, 0:4] = np.arange(n, dtype="<u4").view(np.uint8).reshape(n, 4)
    reports[:, 4] = 0x01
    reports[:, 5] = 0x02
    reports[:, 6:18] = counts.view(np.uint8).reshape(n, 12)

    rows: list[tuple[int, str]] = []
    arrival = t_true + rng.integers(50_000, 300_000, n)
    src = ticks(t_true) if report_src_unit == "ticks" else t_true
    if with_reports:
        for i in np.flatnonzero(keep):
            rows.append((int(arrival[i]), f"{arrival[i]},REPORT,{src[i]},{reports[i].tobytes().hex()}"))
    for t_key in key_times:
        for event, t in (("KEYDOWN", t_key), ("KEYUP", t_key + 90 * MS)):
            key_src = ticks(t) if key_src_scale is None else int(t * key_src_scale)
            a = t + int(rng.integers(100_000, 500_000))
            rows.append((a, f"{a},{event},{key_src},"))
    rows.sort()

    header = [
        "# keyfeel sensor-log format=1",
        "# started_utc=2026-10-21T12:00:00Z",
        "# hw_model=SynthMac1,1",
        "# os=27.0 (synthetic)",
        f"# mach_timebase={TIMEBASE[0]}/{TIMEBASE[1]}",
        f"# euid={euid}",
        f"# label={label}",
        "# device.product=accel",
        "# device.transport=SPU",
        "# device.open=0x00000000 success",
        "# keys.listen_access=granted",
        "# keys.tap=session",
        az.COLUMNS,
    ]
    footer = ["# end.reason=seconds", f"# end.reports={int(keep.sum()) if with_reports else 0}"]
    path.write_text("\n".join(header + [r for _, r in rows] + footer) + "\n", encoding="utf-8")
    return path


class TempDirTest(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self._tmp.name)

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def write(self, name: str, text: str) -> Path:
        path = self.dir / name
        path.write_text(text, encoding="utf-8")
        return path


class ReadLogTests(TempDirTest):
    def test_parses_rows_and_metadata(self) -> None:
        log = az.read_log(synth_log(self.dir / "a.csv", "surface=desk,row=home,force=soft", taps=[0.02] * 3))
        self.assertEqual(log.meta["hw_model"], "SynthMac1,1")
        self.assertEqual(log.tags, {"surface": "desk", "row": "home", "force": "soft"})
        self.assertEqual(log.timebase, TIMEBASE)
        self.assertEqual(log.report_length, 22)
        self.assertEqual(log.keydown_arrival_ns.size, 3)
        self.assertEqual(log.keyup_arrival_ns.size, 3)
        self.assertEqual(log.reports.shape[0], log.report_arrival_ns.size)

    def test_rejects_malformed_files(self) -> None:
        good = "# keyfeel sensor-log format=1\n" + az.COLUMNS + "\n"
        cases = {
            "no-format.csv": az.COLUMNS + "\n1,REPORT,1,00\n",
            "bad-columns.csv": "# keyfeel sensor-log format=1\nt,event\n",
            "bad-hex.csv": good + "1,REPORT,1,zz\n",
            "odd-hex.csv": good + "1,REPORT,1,abc\n",
            "unknown-event.csv": good + "1,KEYPRESS,1,\n",
            "key-data.csv": good + "1,KEYDOWN,1,41\n",
            "fields.csv": good + "1,REPORT,1\n",
            "signed.csv": good + "-1,REPORT,1,00\n",
        }
        for name, text in cases.items():
            with self.subTest(name), self.assertRaises(az.LogError):
                az.read_log(self.write(name, text))
        with self.assertRaises(az.LogError):
            az.read_log(self.dir / "missing.csv")

    def test_keeps_the_most_common_report_length(self) -> None:
        text = "# keyfeel sensor-log format=1\n" + az.COLUMNS + "\n"
        text += "1,REPORT,1,0001\n2,REPORT,2,0002\n3,REPORT,3,000003\n"
        log = az.read_log(self.write("lengths.csv", text))
        self.assertEqual(log.reports.shape, (2, 2))
        self.assertEqual(log.other_length_reports, 1)


class ClockTests(TempDirTest):
    def test_identifies_mach_ticks_on_the_same_clock(self) -> None:
        log = az.read_log(synth_log(self.dir / "c.csv", "force=soft", taps=[0.02] * 6))
        reports = az.fit_clock(log.report_src, log.report_arrival_ns, log.timebase)
        keys = az.fit_clock(log.keydown_src, log.keydown_arrival_ns, log.timebase)
        for fit, low, high in ((reports, 50_000, 300_000), (keys, 100_000, 500_000)):
            self.assertIsNotNone(fit)
            assert fit is not None and fit.delay_ns is not None
            self.assertEqual(fit.unit, "mach ticks")
            self.assertTrue(fit.same_clock)
            self.assertAlmostEqual(fit.ns_per_unit, 125 / 3, delta=0.05)
            self.assertGreaterEqual(fit.delay_ns["min"], low - 100)
            self.assertLessEqual(fit.delay_ns["max"], high + 100)

    def test_identifies_nanoseconds_and_unknown_units(self) -> None:
        log = az.read_log(
            synth_log(self.dir / "n.csv", "force=soft", taps=[0.02] * 6, report_src_unit="ns",
                      key_src_scale=0.137)
        )
        reports = az.fit_clock(log.report_src, log.report_arrival_ns, log.timebase)
        keys = az.fit_clock(log.keydown_src, log.keydown_arrival_ns, log.timebase)
        assert reports is not None and keys is not None
        self.assertEqual(reports.unit, "ns")
        self.assertTrue(reports.same_clock)
        self.assertEqual(keys.unit, "unknown")
        self.assertFalse(keys.same_clock)

    def test_exact_tick_conversion_matches_the_core(self) -> None:
        ticks_ = np.array([0, 1, 2, 3, 24_000_000], dtype=np.int64)
        ns = az.src_to_ns(ticks_, "mach ticks", TIMEBASE)
        self.assertEqual(ns.tolist(), [0, 41, 83, 125, 1_000_000_000])

    def test_mixed_time_bases_fall_back_to_arrival(self) -> None:
        log = az.read_log(
            synth_log(self.dir / "m.csv", "force=soft", taps=[0.02] * 6, key_src_scale=0.137)
        )
        signal = az.build_signal(log, az.Layout("int32", 6), 2.0**-16)
        self.assertEqual(signal.time_base, "arrival")


class RateTests(TempDirTest):
    def test_rate_and_drops(self) -> None:
        log = az.read_log(synth_log(self.dir / "r.csv", "force=rest", seconds=3.0, drop_every=100))
        signal = az.build_signal(log, az.Layout("int32", 6), 2.0**-16)
        assert signal.rate is not None
        self.assertAlmostEqual(signal.rate.hz, 1000.0, delta=5.0)
        self.assertEqual(signal.rate.gaps, 29)
        self.assertEqual(signal.rate.missing, 29)
        self.assertEqual(signal.rate.non_monotonic, 0)

    def test_too_few_samples(self) -> None:
        self.assertIsNone(az.sample_rate(np.array([1, 2], dtype=np.int64)))


class LayoutTests(TempDirTest):
    def test_parse_and_format(self) -> None:
        layout = az.Layout.parse("int16x3@4")
        self.assertEqual((layout.dtype, layout.offset, layout.size), ("int16", 4, 2))
        self.assertEqual(str(layout), "int16x3@4")
        for bad in ("int8x3@0", "int16x6@0", "float32x3@", "int32@6"):
            with self.subTest(bad), self.assertRaises(ValueError):
                az.Layout.parse(bad)
        self.assertFalse(az.Layout("int32", 12).fits(22))
        with self.assertRaises(az.LogError):
            az.Layout("int32", 12).decode(np.zeros((4, 22), dtype=np.uint8))

    def test_candidates_cover_every_type_and_offset(self) -> None:
        candidates = az.candidate_layouts(22)
        self.assertEqual(len(candidates), 17 + 11 + 11)
        self.assertIn(az.Layout("float32", 10), candidates)

    def test_detects_the_synthetic_layout_and_scale(self) -> None:
        log = az.read_log(synth_log(self.dir / "l.csv", "force=soft", taps=[0.05] * 8, seconds=4.0))
        reports, t_ns, keys = az.sorted_rows(log)
        ranked = az.rank_layouts(reports, t_ns, keys)
        best = ranked[0]
        self.assertTrue(best.usable)
        self.assertEqual(str(best.layout), "int32x3@6")
        assert best.scale is not None and best.key_snr is not None
        self.assertEqual(best.scale.nearest, "2^-16")
        self.assertLess(abs(best.scale.error), 0.01)
        self.assertGreater(best.key_snr, 5.0)
        shifted = next(s for s in ranked if str(s.layout) == "int32x3@5")
        self.assertTrue(shifted.shifted_copy_of == "int32x3@6" or not shifted.lsb_live)

    def test_finds_the_counter(self) -> None:
        log = az.read_log(synth_log(self.dir / "k.csv", "force=rest"))
        counters = az.find_counters(log.reports)
        self.assertEqual(counters[0], {"offset": 0, "type": "uint32", "step": 1.0})

    def test_scale_inference(self) -> None:
        scale = az.infer_scale(16384.0)
        assert scale is not None
        self.assertEqual(scale.nearest, "2^-14")
        m_s2 = az.infer_scale(az.STANDARD_GRAVITY)
        assert m_s2 is not None
        self.assertEqual(m_s2.nearest, "1e-0/g (m/s^2)")
        self.assertIsNone(az.infer_scale(0.0))


class EstimatorTests(unittest.TestCase):
    """window_peak() must match kf_velocity_peak_g() boundary for boundary."""

    def setUp(self) -> None:
        # 1 ms samples from 0 to 100 ms; gravity on z.
        self.t = np.arange(0, 101, dtype=np.int64) * MS
        self.xyz = np.tile([0.0, 0.0, 1.0], (self.t.size, 1))

    def test_peak_against_the_baseline(self) -> None:
        self.xyz[80] = [0.3, 0.0, 1.0]
        result = az.window_peak(self.t, self.xyz, 77 * MS, 5 * MS, 8 * MS, 50 * MS, 6)
        assert result is not None
        self.assertAlmostEqual(result[0], 0.3)
        self.assertEqual(result[1], 80 * MS)
        self.assertEqual(result[2], 14)  # [72, 85] ms inclusive.

    def test_window_bounds_are_inclusive_and_baseline_exclusive(self) -> None:
        self.xyz[85] = [0.0, 0.2, 1.0]  # Exactly t_key + POST: inside.
        result = az.window_peak(self.t, self.xyz, 77 * MS, 5 * MS, 8 * MS, 50 * MS, 6)
        assert result is not None
        self.assertAlmostEqual(result[0], 0.2)
        self.xyz[85] = [0.0, 0.0, 1.0]
        self.xyz[71] = [0.0, 0.0, 2.0]  # Last baseline sample: shifts the baseline only.
        result = az.window_peak(self.t, self.xyz, 77 * MS, 5 * MS, 8 * MS, 50 * MS, 6)
        assert result is not None
        self.assertAlmostEqual(result[0], 1.0 / 50.0)  # Baseline [22, 72) ms: z = 1 + 1/50.

    def test_needs_half_the_expected_samples_and_a_baseline(self) -> None:
        self.assertIsNone(az.window_peak(self.t, self.xyz, 77 * MS, 5 * MS, 8 * MS, 50 * MS, 29))
        self.assertIsNotNone(az.window_peak(self.t, self.xyz, 77 * MS, 5 * MS, 8 * MS, 50 * MS, 28))
        self.assertIsNone(az.window_peak(self.t, self.xyz, 2 * MS, 5 * MS, 8 * MS, 50 * MS, 1))

    def test_ceil_ms(self) -> None:
        self.assertEqual(az.ceil_ms(4 * MS), 4 * MS)
        self.assertEqual(az.ceil_ms(4 * MS + 10_000), 4 * MS)  # Within the slack.
        self.assertEqual(az.ceil_ms(4 * MS + 30_000), 5 * MS)


class ReportTests(TempDirTest):
    def make_protocol(self, soft: float, hard: float, spread: float, seed: int = 1) -> list[Path]:
        rng = np.random.default_rng(seed)
        paths = [synth_log(self.dir / "desk-rest.csv", "surface=desk,force=rest", seconds=4.0)]
        for row in ("home", "space"):
            for force, amp in (("soft", soft), ("hard", hard)):
                taps = list(amp * (1.0 + rng.uniform(-spread, spread, 20)))
                paths.append(
                    synth_log(
                        self.dir / f"desk-{row}-{force}.csv",
                        f"surface=desk,row={row},force={force}",
                        taps=taps,
                        seed=int(rng.integers(1 << 30)),
                    )
                )
        return paths

    def test_recovers_peaks_delays_and_windows(self) -> None:
        report = az.build_report(self.make_protocol(0.02, 0.10, 0.1))
        self.assertEqual(report["layout"], "int32x3@6")
        self.assertEqual(report["g_per_unit"], 2.0**-16)
        windows = report["windows"]
        self.assertAlmostEqual(windows["delay_ns"]["p50"] / MS, DELAY_MS, delta=1.2)
        self.assertGreaterEqual(windows["post_ns"], 5 * MS)
        self.assertLessEqual(windows["post_ns"], 7 * MS)
        self.assertEqual(windows["pre_ns"], MS)
        self.assertTrue(windows["post_within_budget"])
        for k in report["keystrokes"]:
            nominal = 0.02 if k["tags"]["force"] == "soft" else 0.10
            self.assertLess(abs(k["peak_g"] / nominal - 1.0), 0.15, k)
            self.assertTrue(k["isolated"])

    def test_gate_and_verdict_when_forces_separate(self) -> None:
        report = az.build_report(self.make_protocol(0.02, 0.10, 0.1))
        gate = {(g["surface"], g["row"]): g["separates"] for g in report["gate"]}
        self.assertEqual(gate, {("desk", "home"): True, ("desk", "space"): True})
        self.assertEqual(report["needs_root"], "no")
        self.assertEqual(report["verdict"]["suggestion"], "go")
        noise = report["noise_floor_g"]["desk"]
        self.assertLess(noise["p95"], 0.005)
        self.assertGreater(noise["n"], 100)

    def test_overlapping_forces_do_not_separate(self) -> None:
        report = az.build_report(self.make_protocol(0.05, 0.06, 0.4))
        self.assertTrue(all(g["separates"] is False for g in report["gate"]))
        self.assertEqual(report["verdict"]["suggestion"], "no-go")

    def test_root_only_logs_mean_root_is_needed(self) -> None:
        perms = [{"euid": "501", "reports": 0}, {"euid": "0", "reports": 10}]
        self.assertEqual(az.needs_root(perms), "yes")
        self.assertEqual(az.needs_root([{"euid": "501", "reports": 0}]).split()[0], "unknown")

    def test_logs_without_reports_still_report_permissions(self) -> None:
        path = synth_log(self.dir / "empty.csv", "probe=user", with_reports=False)
        report = az.build_report([path])
        self.assertIsNone(report["layout"])
        self.assertEqual(report["permissions"][0]["reports"], 0)
        self.assertIn("incomplete", report["verdict"]["suggestion"])
        self.assertIn("Needs root: unknown", az.format_markdown(report))

    def test_markdown_and_json_cli(self) -> None:
        paths = [str(p) for p in self.make_protocol(0.02, 0.10, 0.1)]
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertEqual(az.main(["report", *paths, "--keys"]), 0)
        text = out.getvalue()
        self.assertIn("| desk | home | 20 |", text)
        self.assertIn("## Keystrokes", text)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertEqual(az.main(["report", *paths, "--json"]), 0)
        self.assertEqual(json.loads(out.getvalue())["layout"], "int32x3@6")

    def test_cli_reports_errors(self) -> None:
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            self.assertEqual(az.main(["inspect", str(self.dir / "missing.csv")]), 1)
        self.assertIn("analyze.py:", err.getvalue())


class FixtureTests(TempDirTest):
    def test_fixture_is_small_and_valid(self) -> None:
        source = synth_log(self.dir / "big.csv", "force=rest", seconds=4.0)
        out = self.dir / "fixture.csv"
        rows = az.write_fixture(source, out, 20_000)
        self.assertLessEqual(out.stat().st_size, 20_000)
        self.assertGreater(rows, 100)
        log = az.read_log(out)
        self.assertEqual(log.meta["fixture.source"], "big.csv")
        self.assertEqual(log.reports.shape[0], rows)


class PlotTests(TempDirTest):
    def test_plot_writes_a_png(self) -> None:
        try:
            import matplotlib  # noqa: F401
        except ImportError:
            self.skipTest("matplotlib not installed")
        source = synth_log(self.dir / "p.csv", "force=soft", taps=[0.05] * 4)
        out = self.dir / "p.png"
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(az.main(["plot", str(source), "--out", str(out)]), 0)
        self.assertEqual(out.read_bytes()[:8], b"\x89PNG\r\n\x1a\n")


if __name__ == "__main__":
    unittest.main()
