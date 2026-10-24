import { switches } from "@/lib/content";
import type { SwitchKind } from "@/lib/packs";

/* How each kind of switch pushes back as it travels down: the force curve keyboard people
 * draw. Stylised, not measured. The words come from the shared content. */
const CURVES: Record<SwitchKind, { path: string; at: [number, number] }> = {
  linear: { path: "M22 122 C90 106 180 78 262 34", at: [124, 66] },
  tactile: {
    path: "M22 122 C52 86 74 60 98 58 C116 57 124 92 146 92 C186 90 226 66 262 32",
    at: [74, 26],
  },
  clicky: {
    path: "M22 122 C48 88 70 56 94 50 L104 96 C150 92 214 66 262 32",
    at: [112, 30],
  },
  other: {
    path: "M22 122 C44 66 74 40 106 42 C130 44 140 104 168 100 C204 96 232 70 262 36",
    at: [96, 14],
  },
};

const mono = { fontFamily: "var(--font-mono)" };

export function ForceCurve({ kind, color }: { kind: SwitchKind; color: string }) {
  const c = CURVES[kind];
  const tag = switches.curves[kind];
  const w = tag.length * 8.6 + 22;
  return (
    <svg viewBox="0 0 290 150" className="h-full w-full" aria-hidden="true">
      <path d="M22 10V128H280" stroke="var(--color-ink)" strokeOpacity="0.35" strokeWidth="2" fill="none" />
      <path d="M22 70H280" stroke="var(--color-ink)" strokeOpacity="0.15" strokeWidth="1.4" strokeDasharray="4 6" />
      <text x="28" y="22" fontSize="11" fill="var(--color-mute)" style={mono}>
        {switches.axes.force}
      </text>
      <text x="230" y="144" fontSize="11" fill="var(--color-mute)" style={mono}>
        {switches.axes.travel}
      </text>
      <path d={c.path} stroke={color} strokeWidth="4.5" strokeLinecap="round" fill="none" />
      {kind === "clicky" && (
        <path d="M112 50l12-10M116 62l16-2M110 38l4-14" stroke={color} strokeWidth="2.6" strokeLinecap="round" />
      )}
      <g transform={`translate(${c.at[0]} ${c.at[1]})`}>
        <rect width={w} height="26" rx="13" fill="var(--color-raised)" stroke="var(--color-line)" strokeWidth="1.5" />
        <text x={w / 2} y="17.5" textAnchor="middle" fontSize="13" fontWeight="700" fill="var(--color-ink)" style={mono}>
          {tag}
        </text>
      </g>
    </svg>
  );
}
