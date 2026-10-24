"use client";

import { startAudio } from "@/lib/audio";
import { switches } from "@/lib/content";
import { selectDemoPack } from "@/lib/demo-store";
import { demoPacks, packsOfKind, type SwitchKind } from "@/lib/packs";

import { ArrowRight, Eyebrow, Mark } from "./Bits";
import { kindColor } from "./palette";

/* How each kind of switch pushes back as it travels down: the force curve keyboard people
 * draw. Stylised, not measured. The words come from the shared content. */
const CURVES: Record<SwitchKind, { path: string; at: [number, number] }> = {
  linear: { path: "M22 122 C90 106 180 78 262 34", at: [128, 70] },
  tactile: {
    path: "M22 122 C52 86 74 60 98 58 C116 57 124 92 146 92 C186 90 226 66 262 32",
    at: [96, 26],
  },
  clicky: { path: "M22 122 C48 88 70 56 94 50 L104 96 C150 92 214 66 262 32", at: [104, 18] },
  other: {
    path: "M22 122 C44 66 74 40 106 42 C130 44 140 104 168 100 C204 96 232 70 262 36",
    at: [110, 14],
  },
};

/** The colour behind each card's curve. */
const TOP: Record<SwitchKind, string> = {
  linear: "var(--color-sun-soft)",
  tactile: "var(--color-olive)",
  clicky: "var(--color-tang)",
  other: "var(--color-lilac)",
};

const mono = { fontFamily: "var(--font-mono)" };

function ForceCurve({ kind }: { kind: SwitchKind }) {
  const c = CURVES[kind];
  const tag = switches.curves[kind];
  const w = tag.length * 8.6 + 22;
  return (
    <svg viewBox="0 0 290 150" className="h-full w-full" aria-hidden="true">
      <path d="M22 10V128H280" stroke="var(--color-ink)" strokeWidth="2.2" fill="none" />
      <path d="M22 70H280" stroke="var(--color-ink)" strokeWidth="1.4" strokeDasharray="4 6" opacity="0.35" />
      <text x="28" y="22" fontSize="11" fill="var(--color-ink)" opacity="0.6" style={mono}>
        {switches.axes.force}
      </text>
      <text x="230" y="144" fontSize="11" fill="var(--color-ink)" opacity="0.6" style={mono}>
        {switches.axes.travel}
      </text>
      <path d={c.path} stroke="var(--color-ink)" strokeWidth="4" strokeLinecap="round" fill="none" />
      {kind === "clicky" && (
        <path d="M112 50l12-10M116 62l16-2M110 38l4-14" stroke="var(--color-ink)" strokeWidth="2.6" strokeLinecap="round" />
      )}
      <g transform={`translate(${c.at[0]} ${c.at[1]})`}>
        <rect width={w} height="26" rx="13" fill="#fff" stroke="var(--color-ink)" strokeWidth="2.2" />
        <text x={w / 2} y="17.5" textAnchor="middle" fontSize="13" fontWeight="700" fill="var(--color-ink)" style={mono}>
          {tag}
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
      {switches.hear(name)}
      <span className="transition-transform duration-150 group-hover:translate-x-1">
        <ArrowRight />
      </span>
    </button>
  );
}

export function SwitchCards() {
  return (
    <section id="l-switches" className="border-t-2 border-ink py-28 sm:py-32">
      <div className="mx-auto max-w-[1320px] px-5 sm:px-8">
        <div className="mx-auto max-w-[860px] text-center">
          <Eyebrow>{switches.eyebrow}</Eyebrow>
          <h2 className="mt-6 font-display text-[clamp(42px,6.2vw,84px)] leading-[1] font-extrabold tracking-[-0.035em]">
            {switches.title.before}
            <Mark color="var(--color-lilac)">{switches.title.mark}</Mark>
          </h2>
          <p className="mt-6 text-[clamp(17px,1.5vw,20px)] leading-relaxed text-ink-soft">{switches.body}</p>
        </div>

        <div className="mt-16 grid gap-5 sm:grid-cols-2 xl:grid-cols-4">
          {switches.cards.map((c) => {
            const demo = demoPacks.find((p) => p.id === c.demo);
            return (
              <article
                key={c.kind}
                className="flex flex-col overflow-hidden rounded-[28px] border-2 border-ink transition-transform duration-200 hover:-translate-y-1"
                style={{ background: kindColor[c.kind] }}
              >
                <div className="h-[210px] border-b-2 border-ink p-5" style={{ background: TOP[c.kind] }}>
                  <div className="h-full rounded-[18px] border-2 border-ink bg-white/85 p-3">
                    <ForceCurve kind={c.kind} />
                  </div>
                </div>
                <div className="flex flex-1 flex-col p-6">
                  <div className="flex items-baseline justify-between gap-3">
                    <h3 className="font-display text-[30px] leading-tight font-extrabold tracking-tight">{c.title}</h3>
                    <span className="shrink-0 rounded-full border-2 border-ink bg-white px-2.5 py-0.5 text-[13px] font-bold">
                      {switches.count(packsOfKind(c.kind).length)}
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
