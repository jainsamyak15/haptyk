# Keyfeel website

The landing page for Keyfeel, with one design per appearance:

- **Dark** (`src/components/`): the app's own "Graphite & Signal" look. Pressable 3D keycaps in
  the hero, a switch tester, the typing demo, a drawing of the menu bar panel, a spec sheet of
  measured numbers, an FAQ.
- **Light** (`src/components/light/`): warm paper and bright colours. Googly-eyed keycap
  characters, a pack marquee, highlighted headings, the typing demo, switch-family cards, the
  engine's numbers, an FAQ.

The page holds both; `globals.css` shows the one matching the system appearance, and the
sun/moon button in either nav switches (saved in `localStorage`, reapplied before first
paint by a small script in `src/app/layout.tsx`). Each design has its own element ids (`l-`
for light), so in-page links land in the design that is showing. Next.js 16 (App Router, Turbopack), Tailwind CSS 4, TypeScript. It is
separate from the Mac app and never ships inside it.

## Run it

```sh
cd frontend
npm install
npm run dev          # http://localhost:3000
npm run build        # production build (every route is static)
npm run start        # serve the production build
npm run lint
```

Set `NEXT_TELEMETRY_DISABLED=1` in your shell to keep Next.js from sending usage telemetry.

## The download button

Every "Download" button points at `site.dmgUrl` in `src/lib/site.ts`. By default that is

```
https://github.com/jainsamyak15/haptyk/releases/latest/download/Keyfeel.dmg
```

so it works once a GitHub release has a DMG attached under exactly the name
`Keyfeel.dmg`. `scripts/release.sh` names its output `Keyfeel-<version>.dmg`, so rename
or copy it to `Keyfeel.dmg` when you attach it. Other release assets don't matter, and the
repository has to be public for visitors to download from it.

To host the file somewhere else, set `NEXT_PUBLIC_DMG_URL` at build time:

```sh
NEXT_PUBLIC_DMG_URL=https://cdn.example.com/Keyfeel.dmg npm run build
```

or put a DMG at `public/downloads/Keyfeel.dmg` (git-ignored) and build with
`NEXT_PUBLIC_DMG_URL=/downloads/Keyfeel.dmg`.

The version, size and requirements shown under the buttons also live in `src/lib/site.ts`.
Update them with each release.

## The demo sounds

`public/sounds/<pack>/` holds four medium-force letter takes plus Space, Return and Delete for
four packs, converted from the app's CAF files to 16-bit WAV with `afconvert`:

| Folder | Pack | Source and licence |
| --- | --- | --- |
| `nk-cream` | NK Cream | Mechvibes, MIT, © 2021 Hai Nguyen |
| `holy-panda` | Holy Panda | kbsim, MIT, © Thomas Lai |
| `cherry-mx-blue` | Cherry MX Blue ABS | Mechvibes, MIT, © 2021 Hai Nguyen |
| `topre` | Topre PBT | Mechvibes, MIT, © 2021 Hai Nguyen |

The MIT licence texts ship beside them (`public/sounds/LICENSE-*.txt`), and the footer credits
both projects, as the licence requires. Only MIT packs are used here. Don't add a pack whose
licence or permission covers only the app.

Playback (`src/lib/audio.ts`) mirrors the app: a take that differs from the last one, slight
pitch and level variation, and each key panned to its place on the keyboard. Nothing typed in
the demo leaves the page.

## Layout

| Path | What |
| --- | --- |
| `src/app/page.tsx` | The page, section by section |
| `src/app/globals.css` | Palette, fonts and animations (`@theme`), shared component classes |
| `src/components/` | The dark design, plus `ThemeToggle` |
| `src/components/light/` | The light design |
| `src/lib/typing-demo.ts` | The demo's keyboard and behaviour, shared by both designs |
| `src/lib/faq.ts` | The FAQ answers, shared by both designs |
| `src/lib/site.ts` | Download URL, version, size, requirements |
| `src/lib/packs.ts` | The 25 bundled packs and the 4 demo packs |

Everything honours Reduced Motion: animations stop, and the light design's googly eyes stop
following the pointer.
