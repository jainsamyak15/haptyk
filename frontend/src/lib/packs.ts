/** The 25 bundled packs (Packs/packs.json) and the four the web demo can play. */

export type SwitchKind = "linear" | "tactile" | "clicky" | "other";

export type Pack = { name: string; kind: SwitchKind };

export const packs: readonly Pack[] = [
  { name: "Alpaca", kind: "linear" },
  { name: "Alps SKCM Orange", kind: "tactile" },
  { name: "Blue Alps", kind: "clicky" },
  { name: "Buckling Spring", kind: "other" },
  { name: "Cherry MX Black ABS", kind: "linear" },
  { name: "Cherry MX Black PBT", kind: "linear" },
  { name: "Cherry MX Blue ABS", kind: "clicky" },
  { name: "Cherry MX Blue PBT", kind: "clicky" },
  { name: "Cherry MX Brown ABS", kind: "tactile" },
  { name: "Cherry MX Brown PBT", kind: "tactile" },
  { name: "Cherry MX Red ABS", kind: "linear" },
  { name: "Cherry MX Red PBT", kind: "linear" },
  { name: "EG Crystal Purple", kind: "tactile" },
  { name: "Gateron Baby Kangaroo", kind: "tactile" },
  { name: "Gateron Black Ink", kind: "linear" },
  { name: "Gateron Blue", kind: "clicky" },
  { name: "Gateron Red Ink", kind: "linear" },
  { name: "Holy Panda", kind: "tactile" },
  { name: "IBM Model F", kind: "other" },
  { name: "Kailh Box Navy", kind: "clicky" },
  { name: "Kailh Box White", kind: "clicky" },
  { name: "NK Cream", kind: "linear" },
  { name: "Razer Green", kind: "clicky" },
  { name: "Topre PBT", kind: "other" },
  { name: "Turquoise Tealios", kind: "linear" },
];

export const packsOfKind = (kind: SwitchKind) => packs.filter((p) => p.kind === kind);

/** Stem colour for each kind, as on real switches: red linear, brown tactile, blue clicky. */
export const stemColor: Record<SwitchKind, string> = {
  linear: "var(--color-stem-linear)",
  tactile: "var(--color-stem-tactile)",
  clicky: "var(--color-stem-clicky)",
  other: "var(--color-stem-other)",
};

export const kindLabel: Record<SwitchKind, string> = {
  linear: "Linear",
  tactile: "Tactile",
  clicky: "Clicky",
  other: "Topre & spring",
};

/** Packs the demo plays: real medium-force recordings, all MIT licensed (public/sounds). */
export type DemoPack = {
  id: string;
  name: string;
  kind: SwitchKind;
  blurb: string;
  credit: "mechvibes" | "kbsim";
};

export const demoPacks: readonly DemoPack[] = [
  { id: "nk-cream", name: "NK Cream", kind: "linear", blurb: "Deep and creamy", credit: "mechvibes" },
  { id: "holy-panda", name: "Holy Panda", kind: "tactile", blurb: "Round, bumpy thock", credit: "kbsim" },
  {
    id: "cherry-mx-blue",
    name: "Cherry MX Blue",
    kind: "clicky",
    blurb: "Loud and crisp",
    credit: "mechvibes",
  },
  { id: "topre", name: "Topre", kind: "other", blurb: "Muted, cushioned", credit: "mechvibes" },
];
