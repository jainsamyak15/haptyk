# SPDX-License-Identifier: LicenseRef-Proprietary
# Copyright (c) 2026 The Keyfeel Authors. All rights reserved.
"""The protocol runner must label logs exactly the way analyze.py groups them."""
from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

import analyze as az
import m0_protocol as m0


class ProtocolTests(unittest.TestCase):
    def test_steps_are_unique_and_complete(self) -> None:
        for surface in ("desk", "lap"):
            steps = m0.steps_for(surface)
            names = [s.name for s in steps]
            self.assertEqual(len(names), len(set(names)))
            self.assertEqual(names[0], "rest")
            self.assertEqual(names[-1], "typing")
            self.assertEqual(len(steps), 2 + 2 * len(m0.ROWS))

    def test_labels_round_trip_into_analyze_groups(self) -> None:
        rows_seen = set()
        for step in m0.steps_for("lap"):
            tags = az.parse_tags(m0.label(step.tags))
            self.assertEqual(tags, step.tags)
            self.assertEqual(tags["surface"], "lap")
            self.assertIn(tags["force"], ("rest", "soft", "hard", "typing"))
            if "row" in tags:
                self.assertIn(tags["row"], az.ROW_ORDER)
                rows_seen.add(tags["row"])
                self.assertEqual(step.keys, m0.TAPS)
        self.assertTrue(set(az.GATE_ROWS) <= rows_seen)

    def test_footer_value(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "log.csv"
            path.write_text("t_ns,event,src_ts,data\n# end.reports=12\n# end.keydown=3\n")
            self.assertEqual(m0.footer_value(path, "end.reports"), "12")
            self.assertEqual(m0.footer_value(path, "end.keydown"), "3")
            self.assertIsNone(m0.footer_value(path, "end.keyup"))
            self.assertIsNone(m0.footer_value(Path(tmp) / "missing.csv", "end.reports"))


if __name__ == "__main__":
    unittest.main()
