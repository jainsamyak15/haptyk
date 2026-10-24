"use client";

import { startAudio } from "@/lib/audio";
import { switches } from "@/lib/content";
import { selectDemoPack } from "@/lib/demo-store";
import { demoPacks, packsOfKind, stemColor } from "@/lib/packs";

import { ArrowRight, Overline } from "./Bits";
import { ForceCurve } from "./ForceCurve";

/** Picks a pack in this design's demo, scrolls to it and gives it focus, ready to type. */
function HearButton({ packId, name }: { packId: string; name: string }) {
  return (
    <button
      type="button"
      onClick={() => {
        startAudio(); // Inside the click, so the browser lets audio start.
        selectDemoPack(packId);
        const pad = document.getElementById("try-pad");
        pad?.scrollIntoView({ behavior: "smooth", block: "center" });
        pad?.focus({ preventScroll: true });
      }}
      className="group inline-flex items-center gap-2 text-[16px] font-bold text-signal"
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
    <section id="switches" className="rule-t py-28 sm:py-32">
      <div className="mx-auto max-w-[1280px] px-5 sm:px-8">
        <Overline>{switches.eyebrow}</Overline>
        <h2 className="mt-5 max-w-[1040px] font-display text-[clamp(32px,4.6vw,60px)] leading-[1.05] font-bold tracking-[-0.03em]">
          {switches.title.before}
          <span className="text-signal">{switches.title.mark}</span>
        </h2>
        <p className="mt-5 max-w-[620px] text-[18px] leading-relaxed text-ink-dim">{switches.body}</p>

        <div className="mt-14 grid gap-5 sm:grid-cols-2 xl:grid-cols-4">
          {switches.cards.map((c) => {
            const demo = demoPacks.find((p) => p.id === c.demo);
            return (
              <article
                key={c.kind}
                className="flex flex-col overflow-hidden rounded-[28px] bg-surface edge transition-colors duration-200 hover:bg-raised/60"
              >
                <div className="h-[200px] p-4">
                  <div className="h-full rounded-[18px] bg-well p-3 edge">
                    <ForceCurve kind={c.kind} color={stemColor[c.kind]} />
                  </div>
                </div>
                <div className="flex flex-1 flex-col px-6 pt-2 pb-6">
                  <div className="flex items-baseline justify-between gap-3">
                    <h3 className="flex items-center gap-2.5 font-display text-[26px] leading-tight font-bold tracking-tight">
                      <span className="h-3 w-3 shrink-0 rounded-full" style={{ background: stemColor[c.kind] }} aria-hidden="true" />
                      {c.title}
                    </h3>
                    <span className="shrink-0 rounded-full px-2.5 py-0.5 font-mono text-[12px] text-mute edge">
                      {switches.count(packsOfKind(c.kind).length)}
                    </span>
                  </div>
                  <p className="mt-3 text-[16px] leading-relaxed text-ink-dim">{c.body}</p>
                  <ul className="mt-5 flex flex-wrap gap-2">
                    {c.picks.map((p) => (
                      <li key={p} className="rounded-full bg-raised px-3 py-1 text-[13px] font-medium text-ink edge">
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
