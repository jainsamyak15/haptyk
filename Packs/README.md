# Sound packs

How sounds get from a licensed source into the app. Spec: "Sound pack format" and Appendix B.

## Rules

- Every file comes from a source whose licence allows commercial use inside an app: commissioned
  recordings, CC0, CC-BY (credited in About), or a paid library that covers use in software.
- Never use CC-BY-NC sounds, and never copy or extract audio from another app or from videos.
- Name each pack after what was actually recorded.
- Each pack has a `SOURCES.md` (copy `SOURCES.template.md`) with URL, licence, author, download
  date and the licence text.

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
