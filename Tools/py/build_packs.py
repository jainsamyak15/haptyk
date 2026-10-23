#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Proprietary
# Copyright (c) 2026 The Keyfeel Authors. All rights reserved.
"""Build every bundled pack listed in Packs/packs.json, from its licensed source.

Four kinds of source:

  freesound            a typing recording: an original you downloaded from the source page into
                       Packs/sources/<id>/raw/ wins over the public preview; extract_keystrokes.py
                       cuts it into keystrokes.
  mechvibes            a Mechvibes default pack (MIT): a sprite or per-key files mapped by key
                       code; each key's own sample goes to its key group.
  kbsim                a kbsim switch (MIT): press and release files per key group.
  mechvibes-community  a community pack's zip from mechvibes.com, used with its author's
                       permission, pinned by SHA-256; read like a Mechvibes pack.

Repository files are fetched at the pinned commit, Freesound previews from Freesound's CDN and
community zips from mechvibes.com, all only with --download and only into
Packs/sources/<id>/raw/. Then it writes
Packs/sources/<id>/SOURCES.md (source, licence, the full licence text from Packs/licenses/),
builds the pack with build_pack.py (--derive-tiers fills the tiers a source lacks) into
App/Resources/Packs/, and validates it with kf-cli when that is built.

Usage: build_packs.py [--only ID ...] [--download] [--out App/Resources/Packs]
"""
from __future__ import annotations

import argparse
import contextlib
import hashlib
import io
import json
import re
import shutil
import stat
import subprocess
import sys
import urllib.parse
import urllib.request
import zipfile
from math import gcd
from pathlib import Path

import numpy as np
import soundfile as sf
from scipy.signal import resample_poly

import build_pack
import extract_keystrokes

REPO = Path(__file__).resolve().parents[2]
MANIFEST = REPO / "Packs" / "packs.json"
LICENCES = {"CC0 1.0 Universal": "CC0-1.0.txt", "CC BY 4.0": "CC-BY-4.0.txt",
            "CC BY 3.0": "CC-BY-3.0.txt"}
ORIGINAL_SUFFIXES = {".wav", ".flac", ".aif", ".aiff", ".mp3", ".m4a", ".ogg"}
KF_CLI = REPO / "build" / "debug" / "bin" / "kf-cli"
REPOSITORIES = {"hainguyents13/mechvibes", "tplai/kbsim"}
MAX_TAKES = 10  # Alpha keys: most of what is typed.
MAX_OTHER_TAKES = 4  # Other groups are pressed far less; this keeps the DMG small.
CLIP_RUN = 3  # This many consecutive samples at full scale is real clipping, not a decoder over.
COMMIT = re.compile(r"[0-9a-f]{40}")
SHA256 = re.compile(r"[0-9a-f]{64}")
COMMUNITY_URL = "https://mechvibes.com/sound-packs/"
PERMISSION = "Permission from the author"
GPL3 = "GNU GPL v3"  # Shipped with its text and the author's original files (ADR-0022).
RATE = 48000  # Community packs mix files of several rates: everything is brought to this one.
MAX_ZIP_BYTES = 100 * 1024 * 1024
MAX_ZIP_FILES = 2000

# kbsim: one file per key group and direction (GENERIC_R0..R4 are the five keyboard rows).
KBSIM_FILES = {
    ("alpha", "down"): [f"press/GENERIC_R{i}.mp3" for i in range(5)],
    ("space", "down"): ["press/SPACE.mp3"],
    ("enter", "down"): ["press/ENTER.mp3"],
    ("backspace", "down"): ["press/BACKSPACE.mp3"],
    ("alpha", "up"): ["release/GENERIC.mp3"],
    ("space", "up"): ["release/SPACE.mp3"],
    ("enter", "up"): ["release/ENTER.mp3"],
    ("backspace", "up"): ["release/BACKSPACE.mp3"],
}

# Mechvibes key codes (libuiohook scan codes) of the keys whose group is not alpha.
MECHVIBES_GROUPS = {57: "space", 28: "enter", 3612: "enter", 14: "backspace", 15: "tab",
                    29: "modifier", 42: "modifier", 54: "modifier", 56: "modifier",
                    58: "modifier", 3613: "modifier", 3640: "modifier", 3675: "modifier",
                    3676: "modifier"}
MECHVIBES_ARROWS = {57416, 57419, 57421, 57424, 61000, 61003, 61005, 61008}


def mechvibes_group(code: int) -> str | None:
    """Key group for a Mechvibes key code; None for keys Keyfeel has no group for (F-keys)."""
    if code in MECHVIBES_GROUPS:
        return MECHVIBES_GROUPS[code]
    if 2 <= code <= 13 or 16 <= code <= 27 or 30 <= code <= 41 or 43 <= code <= 53:
        return "alpha"  # Digits, letters and punctuation.
    return "alpha" if code in MECHVIBES_ARROWS else None


# ---------------------------------------------------------------------------- fetching


def source_file(raw: Path, sound_id: int) -> Path | None:
    """The original if one was downloaded (any file named for the sound, not the preview)."""
    if not raw.is_dir():
        return None
    candidates = sorted(p for p in raw.iterdir() if p.suffix.lower() in ORIGINAL_SUFFIXES
                        and str(sound_id) in p.name and p.is_file() and not p.is_symlink())
    originals = [p for p in candidates if "-hq" not in p.name]
    return (originals or candidates or [None])[0]


def allowed(url: str) -> bool:
    if url.startswith(("https://cdn.freesound.org/previews/", COMMUNITY_URL)):
        return True
    m = re.fullmatch(r"https://raw\.githubusercontent\.com/([^/]+/[^/]+)/([0-9a-f]{40})/[^?#]+", url)
    return m is not None and m.group(1) in REPOSITORIES


def download(url: str, dest: Path) -> None:
    """Freesound previews, community zips from mechvibes.com and pinned files of the two
    repositories only; never outside raw/."""
    if not allowed(url) or ".." in url:
        raise SystemExit(f"build_packs: refusing to download from {url}")
    dest.parent.mkdir(parents=True, exist_ok=True)
    request = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0 (Keyfeel build_packs)"})
    with urllib.request.urlopen(request, timeout=120) as response:
        data = response.read()
    if url.endswith(".ogg") and not data.startswith(b"OggS"):
        raise SystemExit(f"build_packs: {url} is not an Ogg file")
    if not data:
        raise SystemExit(f"build_packs: {url} is empty")
    dest.write_bytes(data)


def repo_url(source: dict, name: str) -> str:
    return (f"https://raw.githubusercontent.com/{source['repo']}/{source['commit']}/{source['path']}/"
            f"{urllib.parse.quote(name)}")


def fetch_repo(source: dict, raw: Path, names: list[str], fetch: bool) -> None:
    for name in names:
        dest = raw / name
        if ".." in Path(name).parts or Path(name).is_absolute():
            raise SystemExit(f"build_packs: unsafe file name {name!r}")
        if dest.is_file():
            continue
        if not fetch:
            raise SystemExit(f"build_packs: {dest} is missing (use --download)")
        download(repo_url(source, name), dest)


def safe_unzip(archive: Path, dest: Path) -> list[Path]:
    """Extracts regular files only, never outside `dest`, within size and count limits."""
    dest.mkdir(parents=True, exist_ok=True)
    root = dest.resolve()
    written = []
    with zipfile.ZipFile(archive) as z:
        members = [m for m in z.infolist() if not m.is_dir()]
        if len(members) > MAX_ZIP_FILES or sum(m.file_size for m in members) > MAX_ZIP_BYTES:
            raise SystemExit(f"build_packs: {archive.name} is too large")
        for m in members:
            if stat.S_ISLNK(m.external_attr >> 16):
                raise SystemExit(f"build_packs: {archive.name} holds a symbolic link")
            name = Path(m.filename.replace("\\", "/"))
            if name.is_absolute() or ".." in name.parts or name.parts[0].startswith("__MACOSX"):
                continue
            target = (dest / name).resolve()
            if root not in target.parents:
                raise SystemExit(f"build_packs: {m.filename} escapes the folder")
            target.parent.mkdir(parents=True, exist_ok=True)
            with z.open(m) as src, open(target, "wb") as out:
                shutil.copyfileobj(src, out)
            written.append(target)
    return written


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


# ---------------------------------------------------------------------------- repository packs


def load(path: Path) -> tuple[np.ndarray, int]:
    data, rate = sf.read(str(path), dtype="float64", always_2d=True)
    return data.mean(axis=1), int(rate)


def to_rate(x: np.ndarray, rate: int) -> np.ndarray:
    if rate == RATE:
        return x
    g = gcd(rate, RATE)
    return resample_poly(x, RATE // g, rate // g)


def clipped(x: np.ndarray) -> bool:
    """A real clip: CLIP_RUN consecutive samples at full scale (single overs are decoder noise)."""
    hot = (np.abs(x) >= 0.999).astype(np.int8)
    return hot.size >= CLIP_RUN and bool(np.any(np.convolve(hot, np.ones(CLIP_RUN, np.int8), "valid") == CLIP_RUN))


def spread(items: list, count: int) -> list:
    """At most `count` items, evenly spread over the list (keys from every part of the board)."""
    if len(items) <= count:
        return items
    return [items[int(i)] for i in np.linspace(0, len(items) - 1, count).round()]


def mechvibes_takes(source: dict, raw: Path, fetch: bool) -> tuple[dict, str, int]:
    fetch_repo(source, raw, ["config.json"], fetch)
    config = json.loads((raw / "config.json").read_text(encoding="utf-8"))
    defines = {int(k): v for k, v in config["defines"].items() if v and k.isdigit()}
    takes: dict[tuple[str, str], list[np.ndarray]] = {}
    rate = 0
    if config["key_define_type"] == "single":
        fetch_repo(source, raw, [config["sound"]], fetch)
        x, rate = load(raw / config["sound"])
        seen: dict[str, set] = {}
        for code in sorted(defines):
            group = mechvibes_group(code)
            start, length = defines[code]
            if group is None or (start, length) in seen.setdefault(group, set()):
                continue
            seen[group].add((start, length))
            a, b = int(start * rate / 1000), int((start + length) * rate / 1000)
            if not clipped(x[a:b]):
                takes.setdefault((group, "down"), []).append(x[a:b])
        kind = f"one Ogg sprite, {sum(len(v) for v in seen.values())} distinct key sounds"
    else:
        names = sorted({v for v in defines.values() if isinstance(v, str)})
        fetch_repo(source, raw, names, fetch)
        seen_files: dict[str, set] = {}
        for code in sorted(defines):
            group, name = mechvibes_group(code), defines[code]
            if group is None or not isinstance(name, str) or name in seen_files.setdefault(group, set()):
                continue
            seen_files[group].add(name)
            x, rate = load(raw / name)
            if not clipped(x):
                takes.setdefault((group, "down"), []).append(x)
        kind = f"{len(names)} per-key files"
    return {k: spread(v, MAX_TAKES if k[0] == "alpha" else MAX_OTHER_TAKES)
            for k, v in takes.items()}, kind, rate


def kbsim_takes(source: dict, raw: Path, fetch: bool) -> tuple[dict, str, int]:
    present = {k: v for k, v in KBSIM_FILES.items() if all(f in source["files"] for f in v)}
    fetch_repo(source, raw, [f for files in present.values() for f in files], fetch)
    takes, rate = {}, 0
    for key, files in present.items():
        for name in files:
            x, rate = load(raw / name)
            takes.setdefault(key, []).append(x)
    return takes, f"{sum(len(v) for v in present.values())} MP3 files", rate


def community_takes(folder: Path, used: list[Path] | None = None
                    ) -> dict[tuple[str, str], list[np.ndarray]]:
    """Each key's own sound from a Mechvibes v1 pack in `folder` (sprite or per-key files), at
    RATE. Duplicates, missing files (packs copied from others sometimes name files they lack)
    and clipped sounds are skipped; at most MAX_TAKES alpha and MAX_OTHER_TAKES other takes.
    `used`, when given, collects the files the kept sounds came from, with config.json."""
    configs = sorted(folder.rglob("config.json"))
    if not configs:
        return {}
    base = configs[0].parent
    config = json.loads(configs[0].read_text(encoding="utf-8-sig"))
    defines = {int(k): v for k, v in (config.get("defines") or {}).items() if v and str(k).isdigit()}
    found: dict[tuple[str, str], list[tuple[np.ndarray, Path]]] = {}
    seen: dict[str, set] = {}
    sprite, sprite_path = None, None
    if config.get("key_define_type") == "single":
        sprite_path = base / str(config.get("sound", ""))
        if not sprite_path.is_file():
            return {}
        sprite = load(sprite_path)
    for code in sorted(defines):
        group, value = mechvibes_group(code), defines[code]
        key = tuple(value) if isinstance(value, list) else value
        if group is None or key in seen.setdefault(group, set()):
            continue
        seen[group].add(key)
        if sprite is not None and isinstance(value, list) and len(value) == 2:
            x, rate = sprite
            x, path = x[int(value[0] * rate / 1000):int((value[0] + value[1]) * rate / 1000)], sprite_path
        elif isinstance(value, str) and ".." not in Path(value).parts and (base / value).is_file():
            try:
                (x, rate), path = load(base / value), base / value
            except (RuntimeError, sf.LibsndfileError):
                continue
        else:
            continue
        if len(x) > 32 and not clipped(x):
            found.setdefault((group, "down"), []).append((to_rate(x, rate), path))
    kept = {k: spread(v, MAX_TAKES if k[0] == "alpha" else MAX_OTHER_TAKES) for k, v in found.items()}
    if used is not None:
        used.extend(sorted({configs[0], *(path for v in kept.values() for _, path in v)}))
    return {k: [x for x, _ in v] for k, v in kept.items()}


def write_takes(takes: dict, src: Path, rate: int) -> dict[str, int]:
    """src/<group>/medium/NN.wav and src/<group>/up/NN.wav, as build_pack.py reads them."""
    for group in build_pack.GROUPS:
        if (src / group).is_dir():
            shutil.rmtree(src / group)  # Only the folders this script writes.
    counts = {}
    for (group, direction), sounds in sorted(takes.items()):
        folder = src / group / ("up" if direction == "up" else "medium")
        folder.mkdir(parents=True, exist_ok=True)
        for n, x in enumerate(sounds, 1):
            take = np.concatenate([extract_keystrokes.highpass(x, rate), np.zeros(int(0.3 * rate))])
            sf.write(str(folder / f"{n:02d}.wav"), take, rate, subtype="PCM_24")
        counts[f"{group} {'up' if direction == 'up' else 'down'}"] = len(sounds)
    return counts


# ---------------------------------------------------------------------------- SOURCES.md


def attribution(pack: dict) -> str:
    s = pack["source"]
    if s["licence"].startswith("CC0"):
        return "None"
    if s["licence"] == PERMISSION:
        return (f"{pack['name']}: sounds by {s['author']} (Mechvibes community pack, {s['url']}), "
                f"used with the author's permission")
    if s["licence"] == GPL3:
        return (f"{pack['name']}: sounds by {s['author']} (Mechvibes community pack, {s['url']}), "
                f"GNU GPL v3; cut, normalised and re-encoded, the original files are in the pack")
    if s["licence"] == "MIT":
        return (f"{pack['name']}: sounds from {s['site']} ({s['project_url']}), {s['copyright']}, "
                f"MIT License")
    return (f"{pack['name']}: “{s['title']}” by {s['author']} ({s['url']}), "
            f"{s['licence']} ({s['licence_url']}); cut into single keystrokes and level-normalised")


def licence_text(source: dict) -> str:
    name = source.get("licence_file") or LICENCES[source["licence"]]
    return (REPO / "Packs" / "licenses" / name).read_text(encoding="utf-8").rstrip()


def table(rows: list[tuple[str, str]]) -> str:
    return "\n".join(f"| {k} | {v} |" for k, v in rows)


def sources_md(pack: dict, used: Path, report: dict) -> str:
    """SOURCES.md for a Freesound typing recording."""
    s = pack["source"]
    tiers = report["tiers"]
    settings = report["settings_ms"]
    derived = "None" if len(tiers) == 4 else ", ".join(
        t for t in extract_keystrokes.TIERS if t not in tiers) + " (from medium, build_pack.py --derive-tiers)"
    file_used = ("the original file" if "-hq" not in used.name
                 else "Freesound's public high-quality preview (Ogg Vorbis); the licence covers it. "
                      "Download the original from the source page into raw/ and rerun "
                      "build_packs.py for a lossless build")
    rows = [
        ("Pack id", f"`com.keyfeel.{pack['id']}`"),
        ("Switch / recording", pack["recording"]),
        ("Source", f"{s['site']}, {s['licence']}"),
        ("URL or contract", s["url"]),
        ("Title", s["title"]),
        ("Author", s["author"]),
        ("Licence", s["licence"]),
        ("Licence URL", s["licence_url"]),
        ("Commercial use in an app allowed", "Yes"),
        ("Attribution required (shown in About)", attribution(pack)),
        ("Downloaded or delivered on", s["downloaded"]),
        ("Original file", s["original"]),
        ("File used", file_used),
        ("Derived tiers", derived),
    ]
    return f"""# Sources: {pack['name']}

Generated by `Tools/py/build_packs.py` from `Packs/packs.json`; edit those, not this file.

| Field | Value |
| --- | --- |
{table(rows)}

## Processing

`Tools/py/extract_keystrokes.py` found {report['events']} sound events in {report['seconds']} s and
kept {report['presses_kept']} isolated keystrokes: a press, then its release, with quiet around
them (merge {settings['merge']:g} ms, longest hold {settings['max_hold']:g} ms, quiet {settings['quiet']:g} ms).
Tiers: {report['tiered_by']} ({', '.join(f'{k} {v}' for k, v in tiers.items())} takes), and
{report['release_takes']} release takes. Every keystroke is an alpha key: the other key groups
fall back to alpha's sounds (ADR-0012).
A zero-phase 30 Hz high-pass removed DC offset and rumble first. `Tools/py/build_pack.py` then
trimmed, faded, resampled to 48 kHz, normalised per tier and encoded CAF/ALAC. No other change
was made to the recording.

## Licence text

```
{licence_text(s)}
```
"""


def repo_sources_md(pack: dict, kind: str, counts: dict[str, int]) -> str:
    """SOURCES.md for a pack taken from an MIT-licensed repository."""
    s = pack["source"]
    rows = [
        ("Pack id", f"`com.keyfeel.{pack['id']}`"),
        ("Switch / recording", pack["recording"]),
        ("Source", f"{s['site']} ({s['project_url']}), {s['licence']}"),
        ("URL or contract", s["url"]),
        ("Title", s["title"]),
        ("Author", s["author"]),
        ("Recordist", s["recordist"]),
        ("Licence", f"{s['licence']} ({s['copyright']})"),
        ("Licence URL", s["licence_url"]),
        ("Commercial use in an app allowed", "Yes"),
        ("Attribution required (shown in About)", attribution(pack)),
        ("Downloaded or delivered on", s["downloaded"]),
        ("Original file", f"{kind}, commit {s['commit']}"),
        ("Derived tiers", "soft, hard, slam (from the single recorded force, build_pack.py --derive-tiers)"),
    ]
    takes = ", ".join(f"{k} {v}" for k, v in counts.items())
    return f"""# Sources: {pack['name']}

Generated by `Tools/py/build_packs.py` from `Packs/packs.json`; edit those, not this file.

| Field | Value |
| --- | --- |
{table(rows)}

## Processing

Each key's own sound was taken from the {s['site']} files at commit `{s['commit']}`, by key group
({takes} takes; at most {MAX_TAKES} for alpha keys and {MAX_OTHER_TAKES} for each other group, spread over
the keyboard; any sound with real
clipping, {CLIP_RUN} or more samples in a row at full scale, was left out). A zero-phase 30 Hz
high-pass removed DC offset and rumble. `Tools/py/build_pack.py` then trimmed, faded, resampled
to 48 kHz, normalised per tier, generated the soft, hard and slam tiers from the one recorded
force, and encoded CAF/ALAC.

## Licence text

The {s['site']} repository's licence, which covers its files. Keep this notice with every copy.

```
{licence_text(s)}
```
"""


def community_sources_md(pack: dict, counts: dict[str, int]) -> str:
    """SOURCES.md for a community pack used with its author's permission."""
    s = pack["source"]
    rows = [
        ("Pack id", f"`com.keyfeel.{pack['id']}`"),
        ("Switch / recording", pack["recording"]),
        ("Source", "Mechvibes community pack (mechvibes.com)"),
        ("URL or contract", s["url"]),
        ("Title", s["title"]),
        ("Author", s["author"]),
        ("Licence", s["licence"]),
        ("Permission", s["permission"]),
        ("Commercial use in an app allowed", "Yes"),
        ("Attribution required (shown in About)", attribution(pack)),
        ("Downloaded or delivered on", s["downloaded"]),
        ("Original file", f"{Path(s['zip']).name} from {s['zip']}, SHA-256 {s['sha256']}"),
        ("Derived tiers", "soft, hard, slam (from the single recorded force, build_pack.py --derive-tiers)"),
    ]
    takes = ", ".join(f"{k} {v}" for k, v in counts.items())
    return f"""# Sources: {pack['name']}

Generated by `Tools/py/build_packs.py` from `Packs/packs.json`; edit those, not this file.

| Field | Value |
| --- | --- |
{table(rows)}

## Processing

Each key's own sound was taken from the published zip ({takes} takes; at most {MAX_TAKES} for
alpha keys and {MAX_OTHER_TAKES} for each other group; sounds the pack's config names but does
not contain, and any sound with real clipping, were left out). A zero-phase 30 Hz high-pass
removed DC offset and rumble. `Tools/py/build_pack.py` then trimmed, faded, resampled to 48 kHz,
normalised per tier, generated the soft, hard and slam tiers from the one recorded force, and
encoded CAF/ALAC.

## Permission

{"The pack carries no licence file. " if s["licence"] == PERMISSION else ""}{s['permission']}
""" + (f"""
## Corresponding source

This pack is a modified version of {s['author']}'s pack, conveyed under the GNU GPL v3. The
original files it was made from ship with it in `original/` (with the pack's `config.json` and
`LICENSE.txt`); the changes are listed under Processing above. They remain under the GPL v3.

## Licence text

```
{licence_text(s)}
```
""" if s["licence"] == GPL3 else "")


# ---------------------------------------------------------------------------- build


def build_freesound(pack: dict, src: Path, fetch: bool) -> dict:
    raw = src / "raw"
    sid = pack["source"]["sound_id"]
    used = source_file(raw, sid)
    if used is None:
        if not fetch:
            raise SystemExit(f"build_packs: no recording in {raw} (use --download)")
        used = raw / f"freesound-{sid}-hq.ogg"
        download(pack["source"]["preview"], used)
    e = pack["extract"]
    with contextlib.redirect_stdout(io.StringIO()):
        status = extract_keystrokes.main([str(used), "--out", str(src), "--merge-ms", str(e["merge_ms"]),
                                          "--max-hold-ms", str(e["max_hold_ms"]),
                                          "--quiet-ms", str(e["quiet_ms"])])
    if status != 0:
        raise SystemExit(f"build_packs: no clean keystroke in {used}")
    report = json.loads((src / "extract-report.json").read_text(encoding="utf-8"))
    (src / "SOURCES.md").write_text(sources_md(pack, used, report), encoding="utf-8")
    return {"presses": report["presses_kept"], "tiers": report["tiers"],
            "releases": report["release_takes"]}


def build_repo(pack: dict, src: Path, fetch: bool) -> dict:
    source = pack["source"]
    if source["repo"] not in REPOSITORIES or not COMMIT.fullmatch(source["commit"]):
        raise SystemExit(f"build_packs: {pack['id']}: unknown repository or unpinned commit")
    raw = src / "raw"
    if pack["kind"] == "mechvibes":
        takes, kind, rate = mechvibes_takes(source, raw, fetch)
    else:
        takes, kind, rate = kbsim_takes(source, raw, fetch)
    if not takes.get(("alpha", "down")):
        raise SystemExit(f"build_packs: {pack['id']}: no alpha key sounds")
    counts = write_takes(takes, src, rate)
    (src / "SOURCES.md").write_text(repo_sources_md(pack, kind, counts), encoding="utf-8")
    return {"takes": counts}


def build_community(pack: dict, src: Path, fetch: bool) -> dict:
    source = pack["source"]
    if not source["zip"].startswith(COMMUNITY_URL) or not SHA256.fullmatch(source["sha256"]):
        raise SystemExit(f"build_packs: {pack['id']}: zip not on mechvibes.com or not pinned")
    raw = src / "raw"
    archive = raw / re.sub(r"[^A-Za-z0-9._-]+", "-", Path(source["zip"]).name)
    if not archive.is_file():
        if not fetch:
            raise SystemExit(f"build_packs: {archive} is missing (use --download)")
        download(source["zip"], archive)
    if sha256(archive) != source["sha256"]:
        raise SystemExit(f"build_packs: {archive.name} changed (SHA-256 {sha256(archive)}); refusing")
    unpacked = raw / "pack"
    if unpacked.exists():
        shutil.rmtree(unpacked)  # Only the folder this script unpacks.
    safe_unzip(archive, unpacked)
    used: list[Path] = []
    takes = community_takes(unpacked, used)
    if not takes.get(("alpha", "down")):
        raise SystemExit(f"build_packs: {pack['id']}: no alpha key sounds")
    counts = write_takes(takes, src, RATE)
    (src / "SOURCES.md").write_text(community_sources_md(pack, counts), encoding="utf-8")
    originals = []
    if source["licence"] == GPL3:  # The GPL's corresponding source travels with the pack.
        licence = next(iter(sorted(unpacked.rglob("LICENSE*"))), None)
        originals = sorted({*used, *([licence] if licence else [])})
    return {"takes": counts, "originals": originals, "root": unpacked}


def build_one(pack: dict, out: Path, fetch: bool) -> dict:
    src = REPO / "Packs" / "sources" / pack["id"]
    kind = pack.get("kind", "freesound")
    if kind == "freesound":
        summary = build_freesound(pack, src, fetch)
    elif kind == "mechvibes-community":
        summary = build_community(pack, src, fetch)
    else:
        summary = build_repo(pack, src, fetch)
    author = pack["source"]["author"]
    status = build_pack.main(["--src", str(src), "--id", f"com.keyfeel.{pack['id']}",
                              "--name", pack["name"], "--switch-type", pack["switch_type"],
                              "--author", f"{author} ({pack['source']['site']})"[:128],
                              "--source-url", pack["source"]["url"], "--derive-tiers",
                              "--max-takes", str(MAX_TAKES), "--out", str(out), "--force"])
    if status != 0:
        raise SystemExit(f"build_packs: build_pack.py failed for {pack['id']}")
    folder = out / (re.sub(r"[^A-Za-z0-9]+", "-", pack["name"]).strip("-") + ".kfpack")
    for path in summary.pop("originals", []):
        target = folder / "original" / path.relative_to(summary["root"])
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, target)
    summary.pop("root", None)
    valid = None
    if KF_CLI.exists():
        valid = subprocess.run([str(KF_CLI), "--validate", str(folder)], capture_output=True,
                               text=True).returncode == 0
    size = sum(p.stat().st_size for p in folder.rglob("*") if p.is_file())
    return {"id": pack["id"], "folder": folder.name, **summary, "kib": round(size / 1024),
            "valid": valid}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--only", nargs="*", default=None, help="pack ids to build")
    parser.add_argument("--download", action="store_true", help="fetch missing source files")
    parser.add_argument("--out", type=Path, default=REPO / "App" / "Resources" / "Packs")
    args = parser.parse_args(argv)
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    packs = [p for p in manifest["packs"] if args.only is None or p["id"] in args.only]
    if args.only and len(packs) != len(set(args.only)):
        raise SystemExit("build_packs: unknown pack id in --only")
    failed = 0
    for pack in packs:
        result = build_one(pack, args.out, args.download)
        failed += 0 if result["valid"] in (True, None) else 1
        print(json.dumps(result))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
