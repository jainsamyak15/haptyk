# SPDX-License-Identifier: LicenseRef-Proprietary
# Copyright (c) 2026 The Keyfeel Authors. All rights reserved.
"""Tests for build_packs.py and the committed pack manifest (no network)."""
from __future__ import annotations

import json
import re
import tempfile
import unittest
from pathlib import Path

import numpy as np
import soundfile as sf

import build_pack
import build_packs as bps

RATE = 48000
COMMIT = "0" * 40

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


def tone(ms: float, freq: float = 2000.0) -> np.ndarray:
    t = np.arange(int(RATE * ms / 1000)) / RATE
    return 0.5 * np.sin(2 * np.pi * freq * t) * np.exp(-t / 0.02)


def repo_pack(kind: str) -> dict:
    return {
        "id": "test-repo", "name": "Test Repo", "switch_type": "linear", "kind": kind,
        "recording": "Test switches",
        "source": {"site": "Mechvibes", "project_url": "https://github.com/hainguyents13/mechvibes",
                   "repo": "hainguyents13/mechvibes", "commit": COMMIT, "path": "src/audio/x",
                   "url": "https://github.com/hainguyents13/mechvibes/tree/x", "title": "x",
                   "author": "Hai Nguyen and Mechvibes contributors", "recordist": "Not credited",
                   "licence": "MIT", "licence_file": "MIT-mechvibes.txt",
                   "copyright": "Copyright (c) 2021 Hai Nguyen",
                   "licence_url": "https://github.com/hainguyents13/mechvibes/blob/x/LICENSE",
                   "downloaded": "2026-10-23",
                   "files": sorted(f for files in bps.KBSIM_FILES.values() for f in files)},
    }


class RepositoryPackTests(unittest.TestCase):
    def test_key_groups(self) -> None:
        self.assertEqual(bps.mechvibes_group(57), "space")
        self.assertEqual(bps.mechvibes_group(28), "enter")
        self.assertEqual(bps.mechvibes_group(14), "backspace")
        self.assertEqual(bps.mechvibes_group(15), "tab")
        for code in (29, 42, 54, 56, 3675):
            self.assertEqual(bps.mechvibes_group(code), "modifier")
        for code in (2, 16, 30, 44, 53, 57416):
            self.assertEqual(bps.mechvibes_group(code), "alpha")
        for code in (1, 59, 88, 99999):  # Escape and F-keys: no group.
            self.assertIsNone(bps.mechvibes_group(code))

    def test_clipping_detection(self) -> None:
        clean = tone(50)
        self.assertFalse(bps.clipped(clean))
        over = clean.copy()
        over[100] = 1.0  # One over: decoder noise, kept.
        self.assertFalse(bps.clipped(over))
        hard = clean.copy()
        hard[200:204] = -1.0
        self.assertTrue(bps.clipped(hard))

    def test_spread_keeps_ends_and_count(self) -> None:
        self.assertEqual(bps.spread([1, 2, 3], 10), [1, 2, 3])
        picked = bps.spread(list(range(100)), 10)
        self.assertEqual(len(picked), 10)
        self.assertEqual((picked[0], picked[-1]), (0, 99))

    def test_downloads_are_pinned_and_allow_listed(self) -> None:
        self.assertTrue(bps.allowed(f"https://raw.githubusercontent.com/tplai/kbsim/{COMMIT}/a/b.mp3"))
        self.assertTrue(bps.allowed("https://cdn.freesound.org/previews/1/1_2-hq.ogg"))
        self.assertFalse(bps.allowed("https://raw.githubusercontent.com/tplai/kbsim/master/a.mp3"))
        self.assertFalse(bps.allowed(f"https://raw.githubusercontent.com/evil/repo/{COMMIT}/a.mp3"))
        self.assertFalse(bps.allowed("https://example.com/a.ogg"))
        with tempfile.TemporaryDirectory() as tmp, self.assertRaises(SystemExit):
            bps.fetch_repo(repo_pack("kbsim")["source"], Path(tmp), ["../escape.mp3"], False)
        pack = repo_pack("kbsim")
        pack["source"]["commit"] = "main"
        with tempfile.TemporaryDirectory() as tmp, self.assertRaises(SystemExit):
            bps.build_repo(pack, Path(tmp), False)

    def test_mechvibes_sprite_and_multi(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            raw = Path(tmp)
            sprite = np.concatenate([tone(100, f) for f in (1000, 2000, 3000, 4000, 5000)])
            sprite[int(0.42 * RATE):int(0.42 * RATE) + 8] = 1.0  # Key 60's sound clips.
            sf.write(str(raw / "sound.wav"), sprite, RATE, subtype="FLOAT")
            config = {"key_define_type": "single", "sound": "sound.wav",
                      "defines": {"30": [0, 100], "31": [100, 100], "32": [0, 100],  # Duplicate.
                                  "57": [200, 100], "59": [300, 100], "33": [400, 100],
                                  "x": [0, 1]}}
            (raw / "config.json").write_text(json.dumps(config), encoding="utf-8")
            takes, kind, rate = bps.mechvibes_takes({}, raw, False)
            self.assertEqual(rate, RATE)
            self.assertEqual(len(takes[("alpha", "down")]), 2)  # Duplicate once; clipped key out.
            self.assertEqual(len(takes[("space", "down")]), 1)
            self.assertNotIn(("modifier", "down"), takes)  # F1 has no group.
            self.assertIn("4 distinct key sounds", kind)
            for name in ("q.wav", "space.wav"):
                sf.write(str(raw / name), tone(80), RATE)
            config = {"key_define_type": "multi", "sound": "x",
                      "defines": {"16": "q.wav", "17": "q.wav", "57": "space.wav"}}
            (raw / "config.json").write_text(json.dumps(config), encoding="utf-8")
            takes, kind, _ = bps.mechvibes_takes({}, raw, False)
            self.assertEqual(len(takes[("alpha", "down")]), 1)
            self.assertEqual(kind, "2 per-key files")

    def test_kbsim_layout_and_sources(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp)
            raw = src / "raw"
            for files in bps.KBSIM_FILES.values():
                for name in files:
                    (raw / name).parent.mkdir(parents=True, exist_ok=True)
                    sf.write(str(raw / name), tone(60), RATE, format="MP3")
            pack = repo_pack("kbsim")
            takes, kind, rate = bps.kbsim_takes(pack["source"], raw, False)
            self.assertEqual(len(takes[("alpha", "down")]), 5)
            self.assertEqual(len(takes[("space", "up")]), 1)
            counts = bps.write_takes(takes, src, rate)
            self.assertEqual(counts["alpha down"], 5)
            self.assertEqual(len(list((src / "alpha" / "medium").glob("*.wav"))), 5)
            self.assertEqual(len(list((src / "enter" / "up").glob("*.wav"))), 1)
            text = bps.repo_sources_md(pack, kind, counts)
            (src / "SOURCES.md").write_text(text, encoding="utf-8")
            sources = build_pack.read_sources(src)
            self.assertTrue(sources.licence.startswith("MIT") and sources.commercial)
            self.assertIn("Copyright (c) 2021 Hai Nguyen", sources.attribution)
            self.assertIn("Permission is hereby granted", text)
            # build_pack reads the takes and never the downloads in raw/.
            sf.write(str(raw / "alpha_down_slam_9.wav"), tone(60), RATE)
            collected = build_pack.collect(src, RATE, 10)
            self.assertFalse(any(t.source == "alpha_down_slam_9.wav" for t in collected))
            self.assertEqual(sum(1 for t in collected if t.group == "alpha" and t.direction == "down"), 5)


class CommunityPackTests(unittest.TestCase):
    def community_pack(self, archive: Path) -> dict:
        return {"id": "test-community", "name": "Test Community", "switch_type": "clicky",
                "kind": "mechvibes-community", "recording": "Test switches",
                "source": {"site": "mechvibes.com", "url": "https://mechvibes.com/sound-packs/x/",
                           "zip": "https://mechvibes.com/sound-packs/x/dist/" + archive.name,
                           "sha256": bps.sha256(archive), "title": "x", "author": "someone",
                           "licence": bps.PERMISSION, "permission": "The author said yes.",
                           "downloaded": "2026-10-23"}}

    def test_zip_is_pinned_and_read_like_a_mechvibes_pack(self) -> None:
        import zipfile
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "src"
            (src / "raw").mkdir(parents=True)
            archive = src / "raw" / "test.zip"
            key = Path(tmp) / "k.wav"
            sf.write(str(key), tone(80), 44100)
            with zipfile.ZipFile(archive, "w") as z:
                z.writestr("pack/config.json", json.dumps({"key_define_type": "multi", "defines": {
                    "30": "k.wav", "31": "gone.wav", "57": "k.wav"}}))
                z.write(key, "pack/k.wav")
            pack = self.community_pack(archive)
            result = bps.build_community(pack, src, False)
            self.assertEqual(result["takes"], {"alpha down": 1, "space down": 1})
            text = (src / "SOURCES.md").read_text(encoding="utf-8")
            self.assertIn("The author said yes.", text)
            sources = build_pack.read_sources(src)
            self.assertEqual(sources.licence, bps.PERMISSION)
            self.assertIn("sounds by someone", sources.attribution)
            self.assertEqual(sf.info(str(src / "alpha" / "medium" / "01.wav")).samplerate, bps.RATE)
            archive.write_bytes(archive.read_bytes() + b"tampered")
            with self.assertRaises(SystemExit):
                bps.build_community(pack, src, False)
            pack["source"]["zip"] = "https://example.com/test.zip"
            with self.assertRaises(SystemExit):
                bps.build_community(pack, src, False)

    def test_gpl_pack_ships_its_licence_and_originals(self) -> None:
        import zipfile
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "src"
            (src / "raw").mkdir(parents=True)
            archive = src / "raw" / "gpl.zip"
            key = Path(tmp) / "k.wav"
            sf.write(str(key), tone(80), 44100)
            with zipfile.ZipFile(archive, "w") as z:
                z.writestr("config.json", json.dumps({"key_define_type": "multi", "defines": {
                    "30": "k.wav", "31": "unused-by-config.wav"}}))
                z.write(key, "k.wav")
                z.write(key, "Full recording.wav")  # In the zip, not used: not shipped.
                z.writestr("LICENSE.txt", "GNU GENERAL PUBLIC LICENSE Version 3")
            pack = self.community_pack(archive)
            pack["source"].update({"licence": bps.GPL3, "licence_file": "GPL-3.0.txt",
                                   "permission": "Used under the GPL v3."})
            result = bps.build_community(pack, src, False)
            self.assertEqual(sorted(p.name for p in result["originals"]),
                             ["LICENSE.txt", "config.json", "k.wav"])
            text = (src / "SOURCES.md").read_text(encoding="utf-8")
            self.assertIn("## Corresponding source", text)
            self.assertIn("GNU GENERAL PUBLIC LICENSE", text.split("## Licence text", 1)[1])
            sources = build_pack.read_sources(src)
            self.assertEqual(sources.licence, bps.GPL3)
            self.assertIn("GNU GPL v3", sources.attribution)


class ManifestTests(unittest.TestCase):
    """The committed Packs/packs.json and the packs built from it."""

    def setUp(self) -> None:
        self.manifest = json.loads(bps.MANIFEST.read_text(encoding="utf-8"))

    def test_every_entry_is_licensed_and_consistent(self) -> None:
        ids = set()
        for p in self.manifest["packs"]:
            s = p["source"]
            self.assertNotRegex(s["licence"], r"\bNC\b")
            self.assertTrue(re.fullmatch(r"[a-z0-9-]+", p["id"]), p["id"])
            self.assertNotIn(p["id"], ids)
            ids.add(p["id"])
            self.assertIn(p["switch_type"], build_pack.SWITCH_TYPES)
            self.assertTrue(1 <= len(p["name"]) <= 64)
            if p.get("kind", "freesound") == "freesound":
                self.assertIn(s["licence"], bps.LICENCES, p["id"])
                self.assertIn(f"/sounds/{s['sound_id']}/", s["url"])
                self.assertTrue(s["preview"].startswith(f"https://cdn.freesound.org/previews/"
                                                        f"{s['sound_id'] // 1000}/{s['sound_id']}_"))
                e = p["extract"]
                self.assertTrue(45 <= e["merge_ms"] <= 75 and e["max_hold_ms"] <= 400
                                and e["quiet_ms"] >= 60)
            elif p["kind"] == "mechvibes-community":
                self.assertIn(s["licence"], (bps.PERMISSION, bps.GPL3))
                self.assertTrue(s["zip"].startswith(bps.COMMUNITY_URL))
                self.assertTrue(bps.SHA256.fullmatch(s["sha256"]), p["id"])
                self.assertIn("reports", s["permission"])
                if s["licence"] == bps.GPL3:
                    self.assertTrue((bps.REPO / "Packs" / "licenses" / s["licence_file"]).is_file())
            else:
                self.assertIn(p["kind"], ("mechvibes", "kbsim"))
                self.assertEqual(s["licence"], "MIT")
                self.assertIn(s["repo"], bps.REPOSITORIES)
                self.assertTrue(bps.COMMIT.fullmatch(s["commit"]), p["id"])
                self.assertIn(s["commit"], s["url"])
                licence = (bps.REPO / "Packs" / "licenses" / s["licence_file"]).read_text(encoding="utf-8")
                self.assertIn(s["copyright"], licence)

    def test_built_packs_match_the_manifest(self) -> None:
        packs_dir = bps.REPO / "App" / "Resources" / "Packs"
        for p in self.manifest["packs"]:
            folder = packs_dir / (re.sub(r"[^A-Za-z0-9]+", "-", p["name"]).strip("-") + ".kfpack")
            self.assertTrue(folder.is_dir(), f"{folder.name}: run Tools/py/build_packs.py")
            built = json.loads((folder / "pack.json").read_text(encoding="utf-8"))
            self.assertEqual(built["id"], f"com.keyfeel.{p['id']}")
            self.assertTrue(built["license"].startswith(p["source"]["licence"]))
            self.assertEqual(built["source_url"], p["source"]["url"])
            sources = (folder / "SOURCES.md").read_text(encoding="utf-8")
            self.assertIn("| Commercial use in an app allowed | Yes |", sources)
            if p["source"]["licence"].startswith("CC0"):
                self.assertEqual(built["attribution"], "")
            elif p["source"]["licence"] in (bps.PERMISSION, bps.GPL3):
                self.assertIn(p["source"]["author"], built["attribution"])
                self.assertIn("## Permission", sources)
                if p["source"]["licence"] == bps.GPL3:
                    self.assertTrue((folder / "original" / "LICENSE.txt").is_file())
                    self.assertTrue(any((folder / "original").glob("*.wav")))
            elif p["source"]["licence"] == "MIT":
                self.assertIn(p["source"]["copyright"], built["attribution"])
                self.assertIn("Permission is hereby granted", sources)
            else:
                self.assertIn(p["source"]["author"], built["attribution"])


if __name__ == "__main__":
    unittest.main()
