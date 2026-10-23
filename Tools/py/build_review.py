#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Proprietary
# Copyright (c) 2026 The Keyfeel Authors. All rights reserved.
"""Build Packs/review/: every pack Keyfeel bundles or considered, ready to listen to and judge.

  1-in-the-app/                 the bundled packs (Packs/packs.json), as built
  2-licensed-not-in-the-app/    free, licensed alternatives (Packs/review-candidates.json)
  3-no-licence-ask-first/       free community packs without a licence: never ship these
                                without the author's written permission

Each pack folder holds demo.wav (rendered as the engine plays it: render_demo.py), the files as
published (converted to WAV where macOS can't play them), and SOURCE.txt (link, author, licence,
measured bandwidth). Each section also gets compare-<section>.wav: every pack's name spoken (the
built-in `say`, written to a file), then its close-up keys, one pack after another, with a
.txt of the times. README.md says how to listen; REVIEW.md is a sheet for your verdicts.
Downloads come only from Freesound's CDN, mechvibes.com, OpenGameArt and the pinned Mechvibes
commit, and land only inside Packs/review/ (git-ignored). Never shipped.

Usage: build_review.py [--only-section 1|2|3] [--out Packs/review]
"""
from __future__ import annotations

import argparse
import contextlib
import io
import json
import re
import shutil
import stat
import subprocess
import sys
import tempfile
import time
import urllib.request
import zipfile
from pathlib import Path

import numpy as np
import soundfile as sf

import build_packs as bps
import extract_keystrokes as ek
import render_demo as rd

REPO = bps.REPO
CANDIDATES = REPO / "Packs" / "review-candidates.json"
ALLOWED = ("https://cdn.freesound.org/previews/", "https://mechvibes.com/sound-packs/",
           "https://opengameart.org/sites/default/files/",
           "https://raw.githubusercontent.com/hainguyents13/mechvibes/")
MAX_ZIP_BYTES = 100 * 1024 * 1024
MAX_ZIP_FILES = 2000
AUDIO = {".wav", ".ogg", ".mp3", ".flac", ".aif", ".aiff", ".m4a", ".caf"}
SECTIONS = {"1": "1-in-the-app", "2": "2-licensed-not-in-the-app", "3": "3-no-licence-ask-first"}
SUBTYPE = "PCM_16"  # For listening: the sources are lossy or 16-bit already.
EXTRACT_SETTINGS = (("45", "200", "60"), ("75", "200", "60"), ("75", "400", "60"))


# ---------------------------------------------------------------------------- files


def fetch(url: str, dest: Path) -> Path:
    """Downloads `url` to `dest` once; only from the allowed sites."""
    if not url.startswith(ALLOWED) or ".." in url:
        raise SystemExit(f"build_review: refusing to download from {url}")
    if dest.is_file() and dest.stat().st_size > 0:
        return dest
    dest.parent.mkdir(parents=True, exist_ok=True)
    request = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0 (Keyfeel pack review)"})
    for attempt in range(3):
        try:
            with urllib.request.urlopen(request, timeout=120) as response:
                data = response.read(MAX_ZIP_BYTES + 1)
            break
        except (TimeoutError, OSError):
            if attempt == 2:
                raise
            time.sleep(5 * (attempt + 1))
    if not data or len(data) > MAX_ZIP_BYTES:
        raise SystemExit(f"build_review: {url} is empty or too large")
    dest.write_bytes(data)
    return dest


def safe_unzip(archive: Path, dest: Path) -> list[Path]:
    """Extracts regular files only, never outside `dest`, within size and count limits."""
    dest.mkdir(parents=True, exist_ok=True)
    root = dest.resolve()
    written = []
    with zipfile.ZipFile(archive) as z:
        members = [m for m in z.infolist() if not m.is_dir()]
        if len(members) > MAX_ZIP_FILES or sum(m.file_size for m in members) > MAX_ZIP_BYTES:
            raise SystemExit(f"build_review: {archive.name} is too large")
        for m in members:
            if stat.S_ISLNK(m.external_attr >> 16):
                raise SystemExit(f"build_review: {archive.name} holds a symbolic link")
            name = Path(m.filename.replace("\\", "/"))
            if name.is_absolute() or ".." in name.parts or name.parts[0].startswith("__MACOSX"):
                continue
            target = (dest / name).resolve()
            if root not in target.parents:
                raise SystemExit(f"build_review: {m.filename} escapes the folder")
            target.parent.mkdir(parents=True, exist_ok=True)
            with z.open(m) as src, open(target, "wb") as out:
                shutil.copyfileobj(src, out)
            written.append(target)
    return written


def slug(name: str) -> str:
    cleaned = re.sub(r"[^A-Za-z0-9 ()+.,-]+", "", name.replace("_", " "))
    return re.sub(r"\s+\)", ")", re.sub(r"\s+", " ", cleaned)).strip() or "pack"


def playable(src: Path, dest: Path) -> bool:
    """A WAV copy of an audio file macOS might not play (Ogg Vorbis); False if undecodable."""
    try:
        x, rate = rd.decode(src)
    except Exception:  # noqa: BLE001 - a broken file in someone's pack is just reported.
        return False
    sf.write(str(dest), x, rate, subtype=SUBTYPE)
    return True


# ---------------------------------------------------------------------------- takes


def community_takes(folder: Path) -> rd.Takes:
    """Each key's own sound from a Mechvibes v1 pack (sprite or per-key files); missing or
    clipped sounds are skipped. One recorded force, so everything is medium."""
    configs = sorted(folder.rglob("config.json"))
    if not configs:
        return {}
    base = configs[0].parent
    config = json.loads(configs[0].read_text(encoding="utf-8-sig"))
    defines = {int(k): v for k, v in (config.get("defines") or {}).items() if v and str(k).isdigit()}
    takes: dict[tuple[str, str], list[np.ndarray]] = {}
    seen: dict[str, set] = {}
    sprite = None
    if config.get("key_define_type") == "single":
        path = base / str(config.get("sound", ""))
        if not path.is_file():
            return {}
        sprite = rd.decode(path)
    for code in sorted(defines):
        group, value = bps.mechvibes_group(code), defines[code]
        key = tuple(value) if isinstance(value, list) else value
        if group is None or key in seen.setdefault(group, set()):
            continue
        seen[group].add(key)
        if sprite is not None and isinstance(value, list) and len(value) == 2:
            x, rate = sprite
            x = x[int(value[0] * rate / 1000):int((value[0] + value[1]) * rate / 1000)]
        elif isinstance(value, str) and (base / value).is_file():
            x, rate = rd.decode(base / value)
        else:
            continue
        if len(x) > 32 and not bps.clipped(x):
            takes.setdefault((group, "medium"), []).append(rd.to_rate(x, rate))
    limit = {g: bps.MAX_TAKES if g == "alpha" else bps.MAX_OTHER_TAKES for g, _ in takes}
    return {k: bps.spread(v, limit[k[0]]) for k, v in takes.items()}


def recording_takes(recording: Path) -> rd.Takes:
    """Keystrokes cut from a typing recording, as build_packs.py would for a bundled pack, with
    the settings that bundled packs use, then wider ones (clicky switches, deliberate taps). A
    file of one keystroke is used as it is."""
    x, rate = rd.decode(recording)
    if len(x) / rate <= 1.5:
        return {("alpha", "medium"): [rd.to_rate(x, rate)]}
    for merge, hold, quiet in EXTRACT_SETTINGS:
        with tempfile.TemporaryDirectory() as tmp:
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                status = ek.main([str(recording), "--out", tmp, "--merge-ms", merge,
                                  "--max-hold-ms", hold, "--quiet-ms", quiet])
            if status != 0:
                continue
            takes: rd.Takes = {}
            for tier in (*rd.TIERS, "up"):
                files = sorted((Path(tmp) / "alpha" / tier).glob("*.wav"))
                if files:
                    takes[("alpha", tier)] = [rd.to_rate(*rd.decode(f)) for f in files]
            return takes
    return {}


def folder_takes(folder: Path) -> rd.Takes:
    """Every audio file in a folder as one alpha take: for packs of single keystrokes."""
    files = sorted(p for p in folder.rglob("*") if p.suffix.lower() in AUDIO and p.is_file())
    sounds = []
    for f in files:
        with contextlib.suppress(Exception):
            x, rate = rd.decode(f)
            if 0.01 < len(x) / rate <= 1.5:  # Single keystrokes, not typing recordings.
                sounds.append(rd.to_rate(x, rate))
    return {("alpha", "medium"): bps.spread(sounds, bps.MAX_TAKES)} if sounds else {}


def bandwidth_khz(takes: rd.Takes) -> float | None:
    """Median frequency where each alpha take's spectrum falls 40 dB under its 1-6 kHz level."""
    values = []
    for (group, tier), sounds in takes.items():
        if group != "alpha" or tier == "up":
            continue
        for x in sounds[:10]:
            n = 2048
            x = np.pad(x, (0, max(0, n - len(x))))
            spec = np.mean([np.abs(np.fft.rfft(x[i:i + n] * np.hanning(n))) ** 2
                            for i in range(0, len(x) - n + 1, n // 4)], axis=0) + 1e-20
            db = 10 * np.log10(spec)
            f = np.fft.rfftfreq(n, 1 / rd.RATE)
            ref = np.percentile(db[(f > 1000) & (f < 6000)], 90)
            above = np.where(db > ref - 40)[0]
            if len(above):
                values.append(f[above[-1]] / 1000)
    return round(float(np.median(values)), 1) if values else None


def summary(takes: rd.Takes) -> str:
    downs = sum(len(v) for (g, t), v in takes.items() if t != "up")
    ups = sum(len(v) for (g, t), v in takes.items() if t == "up")
    groups = sorted({g for g, _ in takes})
    return f"{downs} key sounds, {ups} release sounds; groups: {', '.join(groups) or 'none'}"


# ---------------------------------------------------------------------------- output


def write_demo(folder: Path, takes: rd.Takes) -> bool:
    if not rd.resolve(takes, "alpha", "medium"):
        return False
    sf.write(str(folder / "demo.wav"), rd.render(takes), rd.RATE, subtype=SUBTYPE)
    return True


def source_txt(folder: Path, rows: dict[str, str]) -> None:
    folder.joinpath("SOURCE.txt").write_text(
        "\n".join(f"{k}: {v}" for k, v in rows.items() if v) + "\n", encoding="utf-8")


def section_in_app(out: Path) -> list[dict]:
    manifest = json.loads(bps.MANIFEST.read_text(encoding="utf-8"))
    rows = []
    for n, pack in enumerate(manifest["packs"], 1):
        built = REPO / "App" / "Resources" / "Packs" / (
            re.sub(r"[^A-Za-z0-9]+", "-", pack["name"]).strip("-") + ".kfpack")
        folder = out / SECTIONS["1"] / f"{n:02d} {slug(pack['name'])}"
        if folder.exists():
            shutil.rmtree(folder)
        folder.mkdir(parents=True)
        shutil.copytree(built, folder / "pack")
        takes = rd.load_kfpack(built)
        write_demo(folder, takes)
        s = pack["source"]
        bw = bandwidth_khz(takes)
        source_txt(folder, {"Pack": pack["name"], "Switch / recording": pack["recording"],
                            "Source": s["url"], "Author": s["author"], "Licence": s["licence"],
                            "In the app": "yes", "Alpha bandwidth": f"{bw} kHz" if bw else "",
                            "Sounds": summary(takes),
                            "Files": "pack/samples (CAF, plays in Finder), pack/SOURCES.md"})
        rows.append({"section": "1", "folder": folder.name, "name": pack["name"], "author": s["author"],
                     "licence": s["licence"], "link": s["url"], "bw": bw, "sounds": summary(takes)})
        print(f"build_review: 1 {pack['name']}", file=sys.stderr)
    return rows


def candidate_folder(out: Path, section: str, name: str) -> Path:
    folder = out / SECTIONS[section] / slug(name)
    for child in ("demo.wav", "SOURCE.txt"):
        (folder / child).unlink(missing_ok=True)
    folder.mkdir(parents=True, exist_ok=True)
    return folder


def section_licensed(out: Path, candidates: list[dict]) -> list[dict]:
    rows = []
    for c in candidates:
        folder = candidate_folder(out, "2", c["name"])
        if c["kind"] == "freesound":
            raw = fetch(c["preview"], folder / "download" / f"freesound-{c['sound_id']}-hq.ogg")
            playable(raw, folder / "recording.wav")
            takes = recording_takes(raw)
            files = "recording.wav (the whole recording; the preview Freesound serves without a login)"
        elif c["kind"] == "mechvibes":
            source = {"repo": c["repo"], "commit": c["commit"], "path": c["path"]}
            bps.fetch_repo(source, folder / "download", ["config.json"], True)
            config = json.loads((folder / "download" / "config.json").read_text(encoding="utf-8"))
            bps.fetch_repo(source, folder / "download", [config["sound"]], True)
            takes = community_takes(folder / "download")
            files = "download/ (the pack as published)"
        else:
            archive = fetch(c["zip"], folder / "download" / Path(c["zip"]).name)
            safe_unzip(archive, folder / "original")
            takes = folder_takes(folder / "original")
            for f in sorted((folder / "original").rglob("*.ogg")):
                playable(f, f.with_suffix(".wav"))
            files = "original/ (the zip as published)"
        has_demo = write_demo(folder, takes)
        bw = bandwidth_khz(takes)
        source_txt(folder, {"Pack": c["name"], "Source": c["url"], "Author": c["author"],
                            "Licence": c["licence"], "In the app": "no (licensed: could be added)",
                            "Original file": c.get("original", ""),
                            "Alpha bandwidth": f"{bw} kHz" if bw else "",
                            "Sounds": summary(takes) if takes else "no clean keystrokes found",
                            "Demo": "demo.wav" if has_demo else "none (no clean keystrokes)",
                            "Files": files})
        rows.append({"section": "2", "folder": folder.name, "name": c["name"], "author": c["author"],
                     "licence": c["licence"], "link": c["url"], "bw": bw,
                     "sounds": summary(takes) if takes else "no clean keystrokes"})
        print(f"build_review: 2 {c['name']}", file=sys.stderr)
    return rows


def section_unlicensed(out: Path, candidates: list[dict]) -> list[dict]:
    rows = []
    for c in candidates:
        folder = candidate_folder(out, "3", c["name"])
        archive = fetch(c["zip"], folder / "download" / re.sub(r"[^A-Za-z0-9._-]+", "-", Path(c["zip"]).name))
        original = folder / "original"
        if original.exists():
            shutil.rmtree(original)
        safe_unzip(archive, original)
        takes = community_takes(original)
        has_demo = write_demo(folder, takes)
        bw = bandwidth_khz(takes)
        source_txt(folder, {"Pack": c["name"], "Source": c["url"], "Author": c["author"],
                            "Licence": "none stated: ask the author for written permission before any use in Keyfeel",
                            "Contact": "Mechvibes Discord, https://discord.gg/MMVrhWxa4w",
                            "Description": c.get("description", ""),
                            "Alpha bandwidth": f"{bw} kHz" if bw else "",
                            "Sounds": summary(takes) if takes else "no usable key sound: missing or clipping; play original/ directly",
                            "Demo": "demo.wav" if has_demo else "none: play the files in original/",
                            "Files": "original/ (the zip as published on mechvibes.com)"})
        rows.append({"section": "3", "folder": folder.name, "name": c["name"], "author": c["author"],
                     "licence": "none", "link": c["url"], "bw": bw,
                     "sounds": summary(takes) if takes else "no usable key sound (clipping)"})
        print(f"build_review: 3 {c['name']}", file=sys.stderr)
    return rows


README = """# Keyfeel pack review

Every sound pack Keyfeel bundles or considered, to judge by ear. Built by
`Tools/py/build_review.py`; this folder is not committed.

## How to listen

- Start with **compare-1-in-the-app.wav** (and the other two compare files): one file per
  folder, each pack's name spoken, then its keys one at a time. The .txt beside it gives the time
  each pack starts. Every pack is at the same level, so leave the volume alone.
- Then open a pack folder and play **demo.wav**: select it in Finder and press Space, or open it
  in QuickTime. Headphones show the differences best; also try the MacBook speakers you will use.
- Each demo has three parts with a pause between them: **close up** (six letters, then Space,
  Enter and Backspace, slowly: the pack's character), **typing** (a sentence as Keyfeel plays
  it: each key panned to its place, a different take each time, press and release sounds), and
  **force** (one key twice at each force: soft, medium, hard, slam). Every demo uses the same
  sentence and rhythm on purpose, so only the sound differs. Packs in 1-in-the-app have generated
  soft, hard and slam tiers where the source recorded one force; the others only get louder.
- `pack/samples` (bundled packs) and `original/` (community packs) hold the individual sounds.
  2-licensed-not-in-the-app also has `recording.wav`, the whole source recording.

## The three folders

1. **1-in-the-app**: the packs Keyfeel ships today.
2. **2-licensed-not-in-the-app**: free recordings whose licence allows use in Keyfeel, not
   bundled (another recording of the switch measured better, or too few clean keystrokes).
   Any of them can be added: say which.
3. **3-no-licence-ask-first**: free community packs from mechvibes.com with no licence. They may
   not go into Keyfeel unless the author agrees in writing. SOURCE.txt names the author; most are
   on the Mechvibes Discord (https://discord.gg/MMVrhWxa4w).

Record your verdicts in REVIEW.md: keep, drop, or (for folder 3) ask.

## Asking an author

A message such as: "Hi <author>, I'm building Keyfeel, a macOS app that plays keyboard sounds.
May I include your <pack name> pack from mechvibes.com in it, free or paid versions, with credit
to you in the app? A written yes here is enough." Keep their reply (a screenshot is fine) with
the pack's SOURCES.md.
"""


def speech(text: str) -> np.ndarray | None:
    """`text` spoken by macOS's built-in voice, as stereo at RATE; None where `say` is missing."""
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "name.aiff"
        if subprocess.run(["/usr/bin/say", "-o", str(out), text], capture_output=True).returncode:
            return None
        x, rate = rd.decode(out)
    x = rd.to_rate(x, rate)
    x = 0.3 * x / max(1e-9, float(np.max(np.abs(x))))
    return np.stack([x, x], axis=1)


def close_up_seconds() -> float:
    """Where a demo's close-up part ends: the nine single keys and their releases."""
    events, _ = rd.plan()
    return events[17][0] + 0.45


def compare_file(section: Path) -> int:
    """compare-<section>.wav: each pack's spoken name, then its close-up, in folder order."""
    pieces, lines, t = [], [], 0.0
    clip = int(close_up_seconds() * rd.RATE)
    n = 0
    for folder in sorted(p for p in section.iterdir() if (p / "demo.wav").is_file()):
        n += 1
        demo, _ = sf.read(str(folder / "demo.wav"), always_2d=True)
        name = re.sub(r"^\d+ ", "", folder.name)
        voice = speech(f"{n}. {name}")
        lines.append(f"{int(t // 60)}:{t % 60:04.1f}  {n:2d}. {folder.name}")
        for part in ((voice if voice is not None else np.zeros((0, 2))), np.zeros((int(0.3 * rd.RATE), 2)),
                     demo[:clip], np.zeros((int(0.8 * rd.RATE), 2))):
            pieces.append(part)
            t += len(part) / rd.RATE
    if not pieces:
        return 0
    out = section.parent / f"compare-{section.name}.wav"
    sf.write(str(out), np.concatenate(pieces), rd.RATE, subtype=SUBTYPE)
    out.with_suffix(".txt").write_text("Start  Pack\n" + "\n".join(lines) + "\n", encoding="utf-8")
    return n


def review_md(rows: list[dict]) -> str:
    titles = {"1": "1. In the app", "2": "2. Licensed, not in the app",
              "3": "3. No licence: ask the author first"}
    out = ["# Pack review sheet", "",
           "Write your verdict in the last two columns: keep / drop (1 and 2), ask / skip (3).",
           "Bandwidth: where the sound's top end stops (lossy files cut at 13-17 kHz; 20 kHz and",
           "above means nothing audible is lost).", ""]
    for section in ("1", "2", "3"):
        part = [r for r in rows if r["section"] == section]
        if not part:
            continue
        out += [f"## {titles[section]}", "",
                "| Folder | Pack | Author | Licence | Bandwidth | Sounds | Verdict | Notes |",
                "| --- | --- | --- | --- | --- | --- | --- | --- |"]
        for r in part:
            bw = f"{r['bw']} kHz" if r["bw"] else "-"
            out.append(f"| {r['folder']} | [{r['name']}]({r['link']}) | {r['author']} | {r['licence']} "
                       f"| {bw} | {r['sounds']} |  |  |")
        out.append("")
    return "\n".join(out)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--only-section", choices=tuple(SECTIONS), default=None)
    parser.add_argument("--out", type=Path, default=REPO / "Packs" / "review")
    args = parser.parse_args(argv)
    candidates = json.loads(CANDIDATES.read_text(encoding="utf-8"))
    args.out.mkdir(parents=True, exist_ok=True)
    rows: list[dict] = []
    if args.only_section in (None, "1"):
        rows += section_in_app(args.out)
    if args.only_section in (None, "2"):
        rows += section_licensed(args.out, candidates["licensed"])
    if args.only_section in (None, "3"):
        rows += section_unlicensed(args.out, candidates["unlicensed"])
    for key, name in SECTIONS.items():
        if args.only_section in (None, key) and (args.out / name).is_dir():
            compare_file(args.out / name)
    (args.out / "README.md").write_text(README, encoding="utf-8")
    if args.only_section is None:
        (args.out / "REVIEW.md").write_text(review_md(rows), encoding="utf-8")
        (args.out / "review.json").write_text(json.dumps(rows, indent=1), encoding="utf-8")
    print(f"build_review: {len(rows)} packs in {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
