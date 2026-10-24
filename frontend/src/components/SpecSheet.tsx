import type { ReactNode } from "react";

import { site } from "@/lib/site";

import { Overline } from "./Bits";

/* Measured numbers (docs/PERFORMANCE.md) and the app's defaults. Keep them in step. */
const SPECS: [string, string, boolean?][] = [
  ["Sound starts within", "one audio buffer · 2.7 ms", true],
  ["Key to sound queued", "under 1 µs (p99)"],
  ["CPU while idle", "0.0%", true],
  ["CPU at 600 keys/min", "under 1%"],
  ["Memory", "about 16 MB"],
  ["Goes idle after", "30 s without typing"],
  ["Sound packs", "25"],
  ["Keystrokes stored", "none", true],
  ["Mutes on calls", "yes, by default"],
  ["Starts at login", "only if you ask"],
  ["Requires", site.requirements.replace(" · ", ", ")],
  ["Download", site.dmgSize],
];

/** The bars of a barcode, from a fixed pattern (no randomness: this renders on the server). */
const BARS = "3121142113211312411221131221143112".split("").map(Number);

export function SpecSheet() {
  return (
    <section id="specs" className="rule-t py-28 sm:py-32">
      <div className="mx-auto grid max-w-[1280px] gap-14 px-5 sm:px-8 lg:grid-cols-[minmax(0,5fr)_minmax(0,7fr)] lg:items-start">
        <div className="lg:sticky lg:top-32">
          <Overline n="04">Specs</Overline>
          <h2 className="mt-5 font-display text-[clamp(30px,3.4vw,46px)] leading-[1.08] font-bold tracking-[-0.03em]">
            Small, fast, and a little obsessive.
          </h2>
          <p className="mt-5 max-w-[460px] text-[18px] leading-relaxed text-ink-dim">
            Written in C from the audio thread up, and measured at every milestone rather than
            promised.
          </p>
        </div>

        {/* The label on the underside of a keyboard. */}
        <div className="rotate-[0.6deg] rounded-[22px] bg-label p-7 text-label-ink edge-lift sm:p-10">
          <div className="flex items-start justify-between gap-6 border-b-2 border-label-ink pb-5">
            <div>
              <p className="font-display text-[22px] font-bold">
                Keyfeel<span className="text-signal">.</span>
              </p>
              <p className="mt-1 font-mono text-[12px] tracking-[0.12em] uppercase opacity-70">
                Model {site.version} · menu bar sound engine
              </p>
            </div>
            <svg viewBox={`0 0 ${BARS.reduce((a, b) => a + b + 1, 0)} 40`} className="h-10 w-28 shrink-0" aria-hidden="true">
              {BARS.reduce<{ x: number; els: ReactNode[] }>(
                (acc, w, i) => {
                  if (i % 2 === 0) {
                    acc.els.push(<rect key={i} x={acc.x} y="0" width={w} height="40" fill="var(--color-label-ink)" />);
                  }
                  acc.x += w + 1;
                  return acc;
                },
                { x: 0, els: [] },
              ).els}
            </svg>
          </div>
          <dl className="mt-2 font-mono text-[clamp(13px,1.2vw,15px)]">
            {SPECS.map(([label, value, strong]) => (
              <div key={label} className="flex items-end border-b border-label-ink/10 py-3">
                <dt className="shrink-0">{label}</dt>
                <span className="leader" aria-hidden="true" />
                <dd className={`text-right ${strong ? "font-bold text-signal-deep" : "font-medium"}`}>{value}</dd>
              </div>
            ))}
          </dl>
          <p className="mt-5 font-mono text-[11px] tracking-[0.06em] uppercase opacity-60">
            Measured on a MacBook Air, macOS 27 · Apple Silicon only
          </p>
        </div>
      </div>
    </section>
  );
}
