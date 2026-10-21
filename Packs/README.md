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

## Recording spec for commissions

Per key group (alpha, space, enter, backspace, modifier, tab): 6–10 takes at each force (soft,
medium, hard, slam) plus 6 release takes; 48 kHz, 24-bit; microphone 15–30 cm away in the same
spot for every pack; quiet room; 0.5 s of silence between takes.
