import type { SwitchKind } from "./packs";
import { site } from "./site";

/**
 * Every word the page shows. The dark and the light design both render from here, so the two
 * appearances always say the same thing; each design only decides how it looks. Section ids are
 * bare ("try"): the dark design uses them as they are, the light one prefixes "l-".
 */

export type SectionId = "top" | "try" | "switches" | "engine" | "faq";

/** A heading with one emphasised phrase: the light design highlights it, the dark one colours it. */
export type Heading = { before: string; mark: string };

type Link = { to: SectionId; label: string };

export const download = {
  label: "Download for Mac",
  short: "Download",
};

export const nav: { home: string; links: Link[]; tryIt: string } = {
  home: "Keyfeel home",
  links: [
    { to: "switches", label: "Switches" },
    { to: "engine", label: "Under the hood" },
    { to: "faq", label: "FAQ" },
  ],
  tryIt: "Try it",
};

export const hero = {
  lead: "Your MacBook,",
  soft: "with a",
  mark: "mechanical soul.",
  body: `Real recordings of real switches, played the moment you press a key. ${site.packCount} packs, from Cherry MX Blue to Topre, in a tiny menu bar app that does nothing at all while you’re not typing.`,
  tryLink: "or hear it in your browser first",
};

export const packBand = {
  label: "Bundled sound packs",
};

export const tryIt = {
  eyebrow: "Try it right here",
  title: { before: "Go on, ", mark: "press something." } satisfies Heading,
  body: "These are recordings straight from Keyfeel’s packs. Pick a switch, click the keyboard and type. Nothing you type leaves this page.",
  picker: "Switch to play",
  pad: (pack: string) => `Typing demo. Press keys to hear ${pack}.`,
  listening: "Type anything…",
  empty: "Nothing typed yet",
  mute: "Mute",
  unmute: "Unmute",
  start: "Click here, then type",
  loading: (pack: string) => `Loading ${pack}…`,
  playing: (pack: string) => `Playing ${pack}. Keys on the left sound on the left.`,
  ready: (pack: string) => `${pack} is ready to play.`,
  note: "Medium-force takes · headphones recommended",
};

export type SwitchCard = {
  kind: SwitchKind;
  title: string;
  body: string;
  picks: string[];
  /** The demo pack its "Hear" button plays. */
  demo: string;
};

export const switches = {
  eyebrow: "Four kinds of feel",
  title: { before: "Pick a ", mark: "personality." } satisfies Heading,
  body: "Every pack is recorded from a real switch, with its own takes for letters, space, return and delete. Here’s how the four families differ.",
  count: (n: number) => `${n} packs`,
  hear: (pack: string) => `Hear ${pack}`,
  /** The force curve each card draws: its axes, and the word for its shape. Stylised, not measured. */
  axes: { force: "force", travel: "travel →" },
  curves: { linear: "smooth", tactile: "bump", clicky: "click!", other: "thock" } satisfies Record<SwitchKind, string>,
  cards: [
    {
      kind: "linear",
      title: "Linear",
      body: "Smooth from the top all the way down. Deep, creamy and calm.",
      picks: ["NK Cream", "Cherry MX Red", "Gateron Black Ink"],
      demo: "nk-cream",
    },
    {
      kind: "tactile",
      title: "Tactile",
      body: "A bump you can hear halfway down. Rounder, with more character.",
      picks: ["Holy Panda", "Cherry MX Brown", "EG Crystal Purple"],
      demo: "holy-panda",
    },
    {
      kind: "clicky",
      title: "Clicky",
      body: "A crisp click on every press. Loud, bright and proud of it.",
      picks: ["Cherry MX Blue", "Kailh Box White", "Razer Green"],
      demo: "cherry-mx-blue",
    },
    {
      kind: "other",
      title: "Thock & spring",
      body: "Topre's cushioned thock, and the buckling spring of the IBM Model F.",
      picks: ["Topre PBT", "IBM Model F", "Buckling Spring"],
      demo: "topre",
    },
  ] satisfies SwitchCard[],
};

/* Numbers here are measured (docs/PERFORMANCE.md) or are the app's defaults. Keep them in step. */
export const engine = {
  title: { before: "The tiny engine behind every ", mark: "clack." } satisfies Heading,
  body: "Written in C from the audio thread up, and measured at every milestone, not just promised.",
  speed: {
    n: "01",
    title: "Faster than your finger",
    body: "Sound starts within a single 128-frame audio buffer. Getting from your key to a queued sound takes under a microsecond.",
    value: "2.7",
    unit: "ms",
    note: "one buffer at 48 kHz · key to sound queued: under 1 µs (p99, measured)",
  },
  privacy: {
    n: "02",
    title: "Remembers nothing you type",
    body: "A key press picks a sound, and that is all it is used for.",
    used: "Which sound to play",
    never: ["The text you type", "Key codes, once the sound is picked", "App names, window titles, URLs"],
    usedLabel: "(used)",
    neverLabel: "(never kept)",
  },
  idle: {
    n: "03",
    title: "Asleep until you type",
    body: "No timers, no polling. Thirty seconds after your last key, audio stops.",
    idle: "idle",
    typing: "typing",
    value: "0.0%",
    caption: "CPU when idle",
  },
  panning: {
    n: "04",
    title: "Keys sound where they are",
    body: "Each key is panned to its place on the keyboard, so Q comes from the left and P from the right.",
    leftKeys: "QWE",
    rightKeys: "IOP",
    left: "← left ear",
    right: "right ear →",
  },
  panel: {
    n: "05",
    title: "Graphite or Glass",
    body: "A menu bar panel with your packs, volume and stats, in warm Graphite or Apple-style Glass.",
    graphite: "Graphite",
    glass: "Glass",
  },
  calls: {
    n: "06",
    title: "Quiet on calls",
    body: "When an app starts using your microphone, Keyfeel goes quiet on its own. Nobody on the call hears you clacking.",
    status: "Mic in use, sounds paused",
    note: "On by default",
  },
};

export const faq = {
  title: { before: "Questions, ", mark: "answered." } satisfies Heading,
};

export const cta = {
  title: { before: "Ready to hear yourself ", mark: "type?" } satisfies Heading,
};

export const footer: { copyright: string; label: string; links: Link[] } = {
  copyright: "© 2026 The Keyfeel Authors.",
  label: "Footer",
  links: [
    { to: "try", label: "Try it" },
    { to: "switches", label: "Switches" },
    { to: "faq", label: "FAQ" },
  ],
};
