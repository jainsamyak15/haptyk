"use client";

import { startAudio } from "@/lib/audio";
import { selectDemoPack } from "@/lib/demo-store";
import { demoPacks, packsOfKind, type SwitchKind } from "@/lib/packs";

import { ArrowRight, Eyebrow, Mark } from "./Bits";
import { kindColor } from "./palette";

/* How each kind of switch pushes back as it travels down: the force curve keyboard people
 * draw. Stylised, not measured. */
const CURVES: Record<SwitchKind, { path: string; tag: string; at: [number, number] }> = {
  linear: { path: "M22 122 C90 106 180 78 262 34", tag: "smooth", at: [128, 70] },
  tactile: {
    path: "M22 122 C52 86 74 60 98 58 C116 57 124 92 146 92 C186 90 226 66 262 32",
    tag: "bump",
    at: [96, 26],
  },
  clicky: { path: "M22 122 C48 88 70 56 94 50 L104 96 C150 92 214 66 262 32", tag: "click!", at: [104, 18] },
  other: {
    path: "M22 122 C44 66 74 40 106 42 C130 44 140 104 168 100 C204 96 232 70 262 36",
    tag: "thock",
    at: [110, 14],
  },
};

const mono = { fontFamily: "var(--font-mono)" };

function ForceCurve({ kind }: { kind: SwitchKind }) {
  const c = CURVES[kind];
  const w = c.tag.length * 8.6 + 22;
  return (
    <svg viewBox="0 0 290 150" className="h-full w-full" aria-hidden="true">
      <path d="M22 10V128H280" stroke="var(--color-ink)" strokeWidth="2.2" fill="none" />
      <path d="M22 70H280" stroke="var(--color-ink)" strokeWidth="1.4" strokeDasharray="4 6" opacity="0.35" />
      <text x="28" y="22" fontSize="11" fill="var(--color-ink)" opacity="0.6" style={mono}>
        force
      </text>
      <text x="230" y="144" fontSize="11" fill="var(--color-ink)" opacity="0.6" style={mono}>
        travel →
      </text>
      <path d={c.path} stroke="var(--color-ink)" strokeWidth="4" strokeLinecap="round" fill="none" />
      {kind === "clicky" && (
        <path d="M112 50l12-10M116 62l16-2M110 38l4-14" stroke="var(--color-ink)" strokeWidth="2.6" strokeLinecap="round" />
      )}
      <g transform={`translate(${c.at[0]} ${c.at[1]})`}>
        <rect width={w} height="26" rx="13" fill="#fff" stroke="var(--color-ink)" strokeWidth="2.2" />
        <text x={w / 2} y="17.5" textAnchor="middle" fontSize="13" fontWeight="700" fill="var(--color-ink)" style={mono}>
          {c.tag}
        </text>
      </g>
    </svg>
  );
}

/** Picks a pack in this design's demo, scrolls to it and gives it focus, ready to type. */
function HearButton({ packId, name }: { packId: string; name: string }) {
  return (
    <button
      type="button"
      onClick={() => {
        startAudio(); // Inside the click, so the browser lets audio start.
        selectDemoPack(packId);
        const pad = document.getElementById("l-try-pad");
        pad?.scrollIntoView({ behavior: "smooth", block: "center" });
        pad?.focus({ preventScroll: true });
      }}
      className="group inline-flex items-center gap-2 text-[17px] font-bold"
    >
      Hear {name}
      <span className="transition-transform duration-150 group-hover:translate-x-1">
        <ArrowRight />
      </span>
    </button>
  );
}

const CARDS: { kind: SwitchKind; title: string; body: string; picks: string[]; demo: string; top: string }[] = [
  {
    kind: "linear",
    title: "Linear",
    body: "Smooth from the top all the way down. Deep, creamy and calm.",
    picks: ["NK Cream", "Cherry MX Red", "Gateron Black Ink"],
    demo: "nk-cream",
    top: "var(--color-sun-soft)",
  },
  {
    kind: "tactile",
    title: "Tactile",
    body: "A bump you can hear halfway down. Rounder, with more character.",
    picks: ["Holy Panda", "Cherry MX Brown", "EG Crystal Purple"],
    demo: "holy-panda",
    top: "var(--color-olive)",
  },
  {
    kind: "clicky",
    title: "Clicky",
    body: "A crisp click on every press. Loud, bright and proud of it.",
    picks: ["Cherry MX Blue", "Kailh Box White", "Razer Green"],
    demo: "cherry-mx-blue",
    top: "var(--color-tang)",
  },
  {
    kind: "other",
    title: "Thock & spring",
    body: "Topre's cushioned thock, and the buckling spring of the IBM Model F.",
    picks: ["Topre PBT", "IBM Model F", "Buckling Spring"],
    demo: "topre",
    top: "var(--color-lilac)",
  },
];

export function SwitchCards() {
  return (
    <section id="l-switches" className="border-t-2 border-ink py-28 sm:py-32">
      <div className="mx-auto max-w-[1320px] px-5 sm:px-8">
        <div className="mx-auto max-w-[860px] text-center">
          <Eyebrow>Four kinds of feel</Eyebrow>
          <h2 className="mt-6 font-display text-[clamp(42px,6.2vw,84px)] leading-[1] font-extrabold tracking-[-0.035em]">
            Pick a <Mark color="var(--color-lilac)">personality.</Mark>
          </h2>
          <p className="mt-6 text-[clamp(17px,1.5vw,20px)] leading-relaxed text-ink-soft">
            Every pack is recorded from a real switch, with its own takes for letters, space, return
            and delete. Here&rsquo;s how the four families differ.
          </p>
        </div>

        <div className="mt-16 grid gap-5 sm:grid-cols-2 xl:grid-cols-4">
          {CARDS.map((c) => {
            const demo = demoPacks.find((p) => p.id === c.demo);
            const count = packsOfKind(c.kind).length;
            return (
              <article
                key={c.kind}
                className="flex flex-col overflow-hidden rounded-[28px] border-2 border-ink transition-transform duration-200 hover:-translate-y-1"
                style={{ background: kindColor[c.kind] }}
              >
                <div className="h-[210px] border-b-2 border-ink p-5" style={{ background: c.top }}>
                  <div className="h-full rounded-[18px] border-2 border-ink bg-white/85 p-3">
                    <ForceCurve kind={c.kind} />
                  </div>
                </div>
                <div className="flex flex-1 flex-col p-6">
                  <div className="flex items-baseline justify-between gap-3">
                    <h3 className="font-display text-[30px] leading-tight font-extrabold tracking-tight">{c.title}</h3>
                    <span className="shrink-0 rounded-full border-2 border-ink bg-white px-2.5 py-0.5 text-[13px] font-bold">
                      {count} packs
                    </span>
                  </div>
                  <p className="mt-3 text-[17px] leading-relaxed text-ink/80">{c.body}</p>
                  <ul className="mt-5 flex flex-wrap gap-2">
                    {c.picks.map((p) => (
                      <li key={p} className="rounded-full border-2 border-ink bg-white/70 px-3 py-1 text-[13px] font-semibold">
                        {p}
                      </li>
                    ))}
                  </ul>
                  <div className="mt-auto pt-7">{demo && <HearButton packId={demo.id} name={demo.name} />}</div>
                </div>
              </article>
            );
          })}
        </div>
      </div>
    </section>
  );
}
