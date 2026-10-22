# SPDX-License-Identifier: LicenseRef-Proprietary
# Copyright (c) 2026 The Keyfeel Authors. All rights reserved.
"""Tests for build_packs.py and the committed pack manifest (no network)."""
from __future__ import annotations

import json
import re
import tempfile
import unittest
from pathlib import Path

import build_pack
import build_packs as bps

REPORT = {"events": 120, "seconds": 30.5, "presses_kept": 20, "settings_ms": {"merge": 45.0, "max_hold": 200.0, "quiet": 60.0},
          "tiered_by": "single tier (derive the rest)", "tiers": {"medium": 10},
          "release_takes": 8}


def pack(licence: str = "CC0 1.0 Universal") -> dict:
    return {
        "id": "test-switch", "name": "Test Switch", "switch_type": "linear",
        "recording": "A keyboard with test switches",
        "source": {"site": "Freesound", "sound_id": 123456,
                   "url": "https://freesound.org/people/someone/sounds/123456/",
                   "title": "typing.wav", "author": "someone", "licence": licence,
                   "licence_url": "https://creativecommons.org/licenses/by/4.0/",
                   "preview": "https://cdn.freesound.org/previews/123/123456_1-hq.ogg",
                   "original": "Wave (.wav), 48 kHz", "downloaded": "2026-10-23"},
        "extract": {"merge_ms": 45, "max_hold_ms": 200, "quiet_ms": 60},
    }


class BuildPacksTests(unittest.TestCase):
    def test_original_wins_over_preview(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            raw = Path(tmp)
            self.assertIsNone(bps.source_file(raw / "missing", 123456))
            (raw / "freesound-123456-hq.ogg").write_bytes(b"OggS")
            (raw / "notes.txt").write_text("x", encoding="utf-8")
            self.assertEqual(bps.source_file(raw, 123456).name, "freesound-123456-hq.ogg")
            (raw / "123456__someone__typing.wav").write_bytes(b"RIFF")
            self.assertEqual(bps.source_file(raw, 123456).name, "123456__someone__typing.wav")
            self.assertIsNone(bps.source_file(raw, 999))

    def test_download_only_from_freesound_previews(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(SystemExit):
                bps.download("https://example.com/sound.ogg", Path(tmp) / "x.ogg")
            self.assertFalse((Path(tmp) / "x.ogg").exists())

    def test_sources_md_passes_build_pack(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp)
            text = bps.sources_md(pack(), src / "freesound-123456-hq.ogg", REPORT)
            (src / "SOURCES.md").write_text(text, encoding="utf-8")
            sources = build_pack.read_sources(src)
            self.assertEqual(sources.licence, "CC0 1.0 Universal")
            self.assertTrue(sources.commercial)
            self.assertEqual(sources.attribution, "")  # CC0: none required.
            self.assertIn("soft, hard, slam (from medium", text)
            self.assertIn("public high-quality preview", text)
            self.assertIn("CC0 1.0 Universal", text.split("## Licence text", 1)[1])

    def test_cc_by_attribution_and_original_file(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp)
            report = dict(REPORT, tiers={t: 10 for t in ("soft", "medium", "hard", "slam")})
            text = bps.sources_md(pack("CC BY 4.0"), src / "123456__someone__typing.wav", report)
            (src / "SOURCES.md").write_text(text, encoding="utf-8")
            sources = build_pack.read_sources(src)
            self.assertIn("by someone", sources.attribution)
            self.assertIn("CC BY 4.0", sources.attribution)
            self.assertNotIn("|", sources.attribution)
            self.assertIn("| Derived tiers | None |", text)
            self.assertIn("| File used | the original file |", text)
            self.assertIn("Attribution 4.0 International", text)


class ManifestTests(unittest.TestCase):
    """The committed Packs/packs.json and the packs built from it."""

    def setUp(self) -> None:
        self.manifest = json.loads(bps.MANIFEST.read_text(encoding="utf-8"))

    def test_every_entry_is_licensed_and_consistent(self) -> None:
        ids = set()
        for p in self.manifest["packs"]:
            s = p["source"]
            self.assertIn(s["licence"], bps.LICENCES, p["id"])
            self.assertNotRegex(s["licence"], r"\bNC\b")
            self.assertTrue(re.fullmatch(r"[a-z0-9-]+", p["id"]), p["id"])
            self.assertNotIn(p["id"], ids)
            ids.add(p["id"])
            self.assertIn(p["switch_type"], build_pack.SWITCH_TYPES)
            self.assertIn(f"/sounds/{s['sound_id']}/", s["url"])
            self.assertTrue(s["preview"].startswith(f"https://cdn.freesound.org/previews/"
                                                    f"{s['sound_id'] // 1000}/{s['sound_id']}_"))
            self.assertTrue(1 <= len(p["name"]) <= 64)
            e = p["extract"]
            self.assertTrue(45 <= e["merge_ms"] <= 75 and e["max_hold_ms"] <= 400 and e["quiet_ms"] >= 60)

    def test_built_packs_match_the_manifest(self) -> None:
        packs_dir = bps.REPO / "App" / "Resources" / "Packs"
        for p in self.manifest["packs"]:
            folder = packs_dir / (re.sub(r"[^A-Za-z0-9]+", "-", p["name"]).strip("-") + ".kfpack")
            self.assertTrue(folder.is_dir(), f"{folder.name}: run Tools/py/build_packs.py")
            built = json.loads((folder / "pack.json").read_text(encoding="utf-8"))
            self.assertEqual(built["id"], f"com.keyfeel.{p['id']}")
            self.assertEqual(built["license"], p["source"]["licence"])
            self.assertEqual(built["source_url"], p["source"]["url"])
            sources = (folder / "SOURCES.md").read_text(encoding="utf-8")
            self.assertIn("| Commercial use in an app allowed | Yes |", sources)
            if p["source"]["licence"].startswith("CC BY"):
                self.assertIn(p["source"]["author"], built["attribution"])
            else:
                self.assertEqual(built["attribution"], "")


if __name__ == "__main__":
    unittest.main()
