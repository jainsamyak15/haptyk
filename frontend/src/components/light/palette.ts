import type { SwitchKind } from "@/lib/packs";

/** Stem colour for each kind in the light design: red linear, brown tactile, blue clicky. */
export const lightStem: Record<SwitchKind, string> = {
  linear: "var(--color-cherry)",
  tactile: "#9a6a3c",
  clicky: "var(--color-volt)",
  other: "var(--color-ink-mute)",
};

/** Card colour for each kind, shared by the switch cards and the demo's space bar. */
export const kindColor: Record<SwitchKind, string> = {
  linear: "var(--color-sun)",
  tactile: "var(--color-olive)",
  clicky: "var(--color-tang)",
  other: "var(--color-lilac)",
};
