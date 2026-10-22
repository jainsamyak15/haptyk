# Sound packs

How sounds get from a licensed source into the app. Spec: "Sound pack format" and Appendix B.

## Rules

- Every file comes from a source whose licence allows commercial use inside an app: commissioned
  recordings, CC0, CC-BY (credited in About), or a paid library that covers use in software.
- Never use CC-BY-NC sounds, and never copy or extract audio from another app or from videos.
- Name each pack after what was actually recorded.
- Each pack has a `SOURCES.md` (copy `SOURCES.template.md`) with URL, licence, author, download
  date and the licence text.

## `pack.json` (format 1)

`App/Sources/Packs/KFPackManifest.m` enforces this; errors name the offending field.

| Field | Required | Rule |
| --- | --- | --- |
| `format` | yes | `1` |
| `id` | yes | Reverse-DNS, letters, digits, `.` and `-`, at most 100 characters |
| `name` | yes | 1 to 64 characters; what was actually recorded |
| `switch_type` | yes | `linear`, `tactile`, `clicky`, `topre`, `buckling` or `other` |
| `author`, `license` | yes | At most 128 characters |
| `version` | yes | At most 32 characters, for example `1.0.0` |
| `source_url` | no | An `http(s)://` URL, or `null` |
| `attribution` | no | Text the About window must show; empty when none is required |
| `derived` | no | Tiers generated rather than recorded, as `"group/tier"`, e.g. `"alpha/slam"` |
| `groups` | yes | Keys `alpha`, `space`, `enter`, `backspace`, `modifier`, `tab` |

Each group holds `down` (an object of `soft`, `medium`, `hard`, `slam`, each a non-empty list of
paths) and/or `up` (a list of paths). `groups.alpha.down` needs at least one file. Unknown
groups, tiers and keys inside a group are errors.

Paths are relative to the pack folder, use `/`, never contain `..` or `.`, never cross a symbolic
link, and name a regular `.wav` or `.caf` file of at most 2 MB. Audio must be mono, 44.1 or
48 kHz, 16- or 24-bit PCM or ALAC, at most 1.5 s. A pack holds at most 400 files and 8 MB of
decoded audio. Fallbacks (ADR-0012): a missing tier plays the nearest one of the group (the
softer on a tie), a group without key-down sounds plays alpha's, and a missing `up` list means
no release sound.

## Pipeline

```
Packs/sources/<pack-id>/        raw downloads (git-ignored) + SOURCES.md + licence text
        │  Tools/py/build_pack.py: split on silence, trim, 2 ms fade-in, normalise per tier,
        │  --derive-tiers for single-force sources, encode CAF/ALAC with afconvert, write pack.json
        ▼
App/Resources/Packs/<Name>.kfpack/   bundled into the app (Git LFS, ADR-0007)
```

`make test-pack` builds the synthetic "Test Click" pack into `build/packs/` for debug builds.

### Building one pack

1. Put the recordings in `Packs/sources/<pack-id>/`, named by group, tier and direction:
   `alpha/soft/*.wav`, `alpha/up/*.wav`, `space_down_hard.wav` and so on. A file may hold
   several takes with silence between them.
2. Copy `SOURCES.template.md` to `Packs/sources/<pack-id>/SOURCES.md` and fill in every row;
   "Commercial use in an app allowed" must say Yes.
3. Build and check:

   ```sh
   Tools/py/.venv/bin/python Tools/py/build_pack.py --src Packs/sources/<pack-id> \
       --id com.keyfeel.<pack-id> --name "<Name>" --switch-type linear [--derive-tiers]
   build/debug/bin/kf-cli --validate "App/Resources/Packs/<Name>.kfpack"
   build/debug/bin/kf-cli --pack "App/Resources/Packs/<Name>.kfpack"   # listen while typing
   ```

4. Commit the pack folder (CAF files through Git LFS, ADR-0007) with its `SOURCES.md`.

Start with one pack of each switch type (linear, tactile, clicky, topre, buckling), as the spec
asks, and get the owner's approval of the sound before the rest. Release builds bundle only
what is in `App/Resources/Packs/`; until then a release app has no sounds.

## Recording spec for commissions

Per key group (alpha, space, enter, backspace, modifier, tab): 6–10 takes at each force (soft,
medium, hard, slam) plus 6 release takes; 48 kHz, 24-bit; microphone 15–30 cm away in the same
spot for every pack; quiet room; 0.5 s of silence between takes.

## Bundled packs (v1)

Eleven packs, each named after what was actually recorded (spec: "drop or rename any of the 16
that can't be sourced legally"), built from Freesound recordings whose licence allows commercial
use in an app. `Packs/packs.json` is the source of truth; each `Packs/sources/<id>/SOURCES.md`
holds the paper trail and the full licence text (ADR-0019).

| Pack | Switch type | Recording | Author | Licence | Tiers |
| --- | --- | --- | --- | --- | --- |
| Alps SKCM Orange | tactile | Apple M0118 (1987) | robni7 | CC0 1.0 | 1 recorded, 3 derived |
| Buckling Spring | buckling | Unicomp | SamsterBirdies | CC0 1.0 | 4 by loudness |
| Cherry MX Black | linear | Leopold FC980M PD | ujonathan | CC0 1.0 | 4 by loudness |
| Cherry MX Blue | clicky | MX Blue keyboard | SamsterBirdies | CC0 1.0 | 4 by loudness |
| Cherry MX Brown | tactile | MX Brown keyboard | majod | CC0 1.0 | 4 by loudness |
| Cherry MX Red | linear | Corsair K95, 96 kHz raw | handygaber | CC0 1.0 | 4 by loudness |
| Gateron Baby Kangaroo | tactile | Nuphy 95 | redpanda69 | CC0 1.0 | 4 by loudness |
| Gateron Blue | clicky | Anne Pro 2, PBT keycaps | forfii | CC0 1.0 | 4 by loudness |
| Holy Panda | tactile | custom board, iPhone recording | leakwok | CC BY 4.0 (credited in About) | 4 by loudness |
| IBM Model F | buckling | 1981 IBM Model F | nakkivene66 | CC0 1.0 | 1 recorded, 3 derived |
| Topre (HHKB) | topre | Happy Hacking Keyboard | grcekh | CC0 1.0 | 4 by loudness |

Not sourced, because no recording with a licence allowing commercial use was found (Freesound,
Wikimedia Commons, OpenGameArt and community packs searched on 2026-10-23): Alpaca, Biscuit, EG
Crystal Purple, Gateron Black Ink, Gateron Red Ink V2, Kailh Box Navy, NK Cream and Turquoise
Tealios; Blue Alps is replaced by Alps SKCM Orange. Keyboard-sound apps and Mechvibes packs that
carry these names give no recording credits or licence, so they are not used (Appendix B). The
Freesound Kailh Box Jade and Gateron Black clips hold too few clean keystrokes for a pack. These
need commissioned recordings (spec below).

All keystrokes are alpha keys; the other groups fall back to alpha (ADR-0012). Tiers "by
loudness" are the typist's own natural variation, split into quartiles: recorded, not generated,
but not force-controlled either. Commissioned four-force recordings remain the best source.

## From a typing recording (`build_packs.py`)

Free libraries mostly hold continuous typing, not take-by-take sessions.
`Tools/py/extract_keystrokes.py` finds each keystroke's press and its release, keeps only clean
ones (isolated by quiet, well above the noise, typical in level and colour), removes DC and
rumble with a 30 Hz high-pass, and writes the `alpha/<tier>/` and `alpha/up/` layout that
`build_pack.py` reads. `Tools/py/build_packs.py` (`make packs`) does every pack in
`Packs/packs.json`: finds the recording in `Packs/sources/<id>/raw/` (downloading the public
preview with `--download` when there is none), extracts, writes `SOURCES.md`, builds and
validates with `kf-cli --validate`.

To add a pack from a licensed recording: add an entry to `Packs/packs.json` (source URL, author,
exact licence, what was recorded, extraction settings), put the file in
`Packs/sources/<id>/raw/`, run `make packs`, listen, and commit the pack with its `SOURCES.md`.
Clicky switches need `merge_ms` 60 to 75, since the click and the bottom-out are two transients.

### Lossless originals

The bundled packs are built from Freesound's public high-quality previews (Ogg Vorbis), which the
same licence covers. For a lossless build, sign in to Freesound, download each original from the
URL in `Packs/packs.json` into `Packs/sources/<id>/raw/` (keep the sound id in the file name,
as Freesound does), and run `make packs`: an original always wins over the preview, and
`SOURCES.md` records which was used.
