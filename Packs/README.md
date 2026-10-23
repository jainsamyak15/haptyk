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

Nineteen packs: fifteen of the spec's sixteen plus four extra switches, each named after what was
recorded. `Packs/packs.json` is the source of truth; each `Packs/sources/<id>/SOURCES.md` holds
the source, licence and full licence text (ADR-0019, ADR-0020).

| Pack | Type | Source | Licence | Sounds |
| --- | --- | --- | --- | --- |
| Alpaca | linear | kbsim | MIT | 5 alpha, space, enter, backspace; releases |
| Alps SKCM Orange | tactile | Freesound, robni7 (Apple M0118) | CC0 1.0 | 10 alpha; releases |
| Blue Alps | clicky | kbsim | MIT | 5 alpha, space, enter, backspace; releases |
| Buckling Spring | buckling | Freesound, SamsterBirdies (Unicomp) | CC0 1.0 | 30 alpha in 4 tiers; releases |
| Cherry MX Black PBT | linear | Mechvibes | MIT | 10 alpha, space, enter, backspace, tab, 9 modifiers |
| Cherry MX Blue PBT | clicky | Mechvibes | MIT | as above |
| Cherry MX Brown PBT | tactile | Mechvibes | MIT | as above |
| Cherry MX Red PBT | linear | Mechvibes | MIT | as above |
| EG Crystal Purple | tactile | Mechvibes | MIT | as above |
| Gateron Baby Kangaroo | tactile | Freesound, redpanda69 (Nuphy 95) | CC0 1.0 | 24 alpha in 4 tiers; releases |
| Gateron Black Ink | linear | kbsim | MIT | 5 alpha, space, enter, backspace; releases |
| Gateron Blue | clicky | Freesound, forfii (Anne Pro 2) | CC0 1.0 | 40 alpha in 4 tiers; releases |
| Gateron Red Ink | linear | kbsim | MIT | 5 alpha, space, enter, backspace; releases |
| Holy Panda | tactile | kbsim | MIT | 5 alpha, space, enter, backspace; releases |
| IBM Model F | buckling | Freesound, nakkivene66 (1981 Model F) | CC0 1.0 | 7 alpha; releases |
| Kailh Box Navy | clicky | kbsim | MIT | 5 alpha, space, enter, backspace; releases |
| NK Cream | linear | Mechvibes ("original by Ryan") | MIT | 10 alpha, space, enter, backspace, tab, 8 modifiers |
| Topre PBT | topre | Mechvibes (Topre Purple Hybrid) | MIT | 10 alpha, space, enter, backspace, tab, 9 modifiers |
| Turquoise Tealios | linear | kbsim | MIT | 5 alpha, space, enter, backspace; releases |

Missing: **Biscuit**, for which no pack or recording exists in any of the sources. "Gateron Red
Ink V2" is named "Gateron Red Ink" because kbsim does not state the version. Mechvibes and kbsim
packs record one force, so build_pack.py derives the soft, hard and slam tiers; the Freesound
packs' tiers are the typist's natural loudness variation (see below). Keys without their own
sound fall back to alpha (ADR-0012).

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

The five Freesound packs are built from Freesound's public high-quality previews (Ogg Vorbis),
which the same licence covers. For a lossless build, sign in to Freesound, download each original from the
URL in `Packs/packs.json` into `Packs/sources/<id>/raw/` (keep the sound id in the file name,
as Freesound does), and run `make packs`: an original always wins over the preview, and
`SOURCES.md` records which was used.
