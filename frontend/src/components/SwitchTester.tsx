"use client";

import { useState } from "react";

import { hit } from "@/lib/audio";
import { selectDemoPack, useDemoPack } from "@/lib/demo-store";
import { demoPacks, kindLabel, packsOfKind, stemColor, type SwitchKind } from "@/lib/packs";

import { ArrowRight, Overline } from "./Bits";
import { ForceCurve } from "./ForceCurve";

const ABOUT: Record<SwitchKind, string> = {
  linear: "Smooth from the top all the way down. Deep, creamy and calm.",
  tactile: "A bump you can hear halfway down. Rounder, with more character.",
  clicky: "A crisp click on every press. Loud, bright and proud of it.",
  other: "Topre's cushioned thock, and the buckling spring of the IBM Model F.",
};

const FAMILIES: SwitchKind[] = ["linear", "tactile", "clicky", "other"];

/** A switch tester: press a switch to hear its pack. The pick carries over to the demo. */
export function SwitchTester() {
  const selected = useDemoPack();
  const [down, setDown] = useState<string | null>(null);
  const pack = demoPacks.find((p) => p.id === selected) ?? demoPacks[0];

  return (
    <section id="switches" className="rule-t py-28 sm:py-32">
      <div className="mx-auto max-w-[1280px] px-5 sm:px-8">
        <Overline n="01">Switches</Overline>
        <h2 className="mt-5 max-w-[1040px] font-display text-[clamp(32px,4.6vw,60px)] leading-[1.05] font-bold tracking-[-0.03em]">
          Four families. Press one.
        </h2>
        <p className="mt-5 max-w-[620px] text-[18px] leading-relaxed text-ink-dim">
          Every pack is recorded from a real switch, with its own takes for letters, space, return
          and delete. Press a switch on the tester to hear it.
        </p>

        <div className="mt-14 grid gap-6 lg:grid-cols-[minmax(0,1fr)_minmax(0,1fr)]">
          {/* The tester board. */}
          <div className="rounded-[30px] bg-surface p-5 edge sm:p-7">
            <div className="grid grid-cols-2 gap-4 sm:gap-5">
              {demoPacks.map((p) => {
                const on = p.id === selected;
                const isDown = down === p.id;
                return (
                  <button
                    key={p.id}
                    type="button"
                    aria-pressed={on}
                    onPointerDown={(e) => {
                      e.preventDefault();
                      setDown(p.id);
                      selectDemoPack(p.id);
                      void hit(p.id, "key");
                    }}
                    onPointerUp={() => setDown(null)}
                    onPointerLeave={() => setDown((d) => (d === p.id ? null : d))}
                    onKeyDown={(e) => {
                      if ((e.key === " " || e.key === "Enter") && !e.repeat) {
                        e.preventDefault();
                        selectDemoPack(p.id);
                        void hit(p.id, "key");
                      }
                    }}
                    className={`group flex flex-col items-center rounded-[22px] p-4 transition-colors sm:p-5 ${
                      on ? "bg-raised ring-2 ring-signal" : "edge hover:bg-raised/60"
                    }`}
                  >
                    {/* Housing, top-down, with the stem in the middle. */}
                    <span className="relative grid aspect-square w-full max-w-[150px] place-items-center rounded-[18px] bg-well edge">
                      <span className="absolute inset-[14%] rounded-[12px] ring-1 ring-ink/10" />
                      <svg
                        viewBox="0 0 24 24"
                        className="w-[44%] transition-transform duration-75"
                        style={{ transform: isDown ? "scale(0.82)" : "scale(1)" }}
                        aria-hidden="true"
                      >
                        <path d="M9.5 2.5h5v7h7v5h-7v7h-5v-7h-7v-5h7z" fill={stemColor[p.kind]} />
                      </svg>
                    </span>
                    <span className="mt-4 font-display text-[clamp(14px,1.4vw,17px)] font-bold">{p.name}</span>
                    <span className="mt-1 font-mono text-[12px] text-mute uppercase">{kindLabel[p.kind]}</span>
                  </button>
                );
              })}
            </div>
          </div>

          {/* What the selected switch is like. */}
          <div className="flex flex-col rounded-[30px] bg-surface p-7 edge sm:p-9">
            <p className="font-mono text-[13px] text-mute uppercase" aria-live="polite">
              Now playing · {kindLabel[pack.kind]}
            </p>
            <h3 className="mt-3 font-display text-[clamp(28px,3vw,40px)] leading-tight font-bold tracking-tight">
              {pack.name}
            </h3>
            <p className="mt-3 text-[17px] leading-relaxed text-ink-dim">{ABOUT[pack.kind]}</p>
            <div className="mt-6 h-[190px] rounded-[20px] bg-well p-4 edge">
              <ForceCurve kind={pack.kind} color={stemColor[pack.kind]} />
            </div>
            <a
              href="#try"
              className="group mt-7 inline-flex items-center gap-2 self-start text-[16px] font-bold text-signal"
              onClick={() => {
                window.setTimeout(() => document.getElementById("try-pad")?.focus({ preventScroll: true }), 450);
              }}
            >
              Type with {pack.name}
              <span className="transition-transform duration-150 group-hover:translate-x-1">
                <ArrowRight />
              </span>
            </a>
          </div>
        </div>

        {/* Every bundled pack, by family. */}
        <div className="mt-16">
          <p className="font-mono text-[13px] text-mute uppercase">All 25 packs in the app</p>
          <div className="mt-6 grid gap-8 sm:grid-cols-2 lg:grid-cols-4">
            {FAMILIES.map((kind) => (
              <div key={kind}>
                <p className="flex items-center gap-2.5 font-display text-[15px] font-bold">
                  <span className="h-3 w-3 rounded-full" style={{ background: stemColor[kind] }} aria-hidden="true" />
                  {kindLabel[kind]}
                  <span className="font-mono text-[12px] font-normal text-mute">{packsOfKind(kind).length}</span>
                </p>
                <ul className="mt-3 space-y-1.5 text-[15px] text-ink-dim">
                  {packsOfKind(kind).map((p) => (
                    <li key={p.name}>{p.name}</li>
                  ))}
                </ul>
              </div>
            ))}
          </div>
        </div>
      </div>
    </section>
  );
}
