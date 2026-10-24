import type { ReactNode } from "react";

import { engine } from "@/lib/content";

import { Cap } from "./Bits";

/* The pieces of the dark design's "under the hood" cards. */

const mono = { fontFamily: "var(--font-mono)" };

export function Card({
  n,
  title,
  body,
  className = "",
  children,
}: {
  n: string;
  title: string;
  body: ReactNode;
  className?: string;
  children: ReactNode;
}) {
  return (
    <article className={`flex flex-col overflow-hidden rounded-[30px] bg-surface p-7 edge sm:p-8 ${className}`}>
      <span className="font-mono text-[13px] font-medium text-signal">{n}</span>
      <h3 className="mt-2 font-display text-[clamp(24px,2.2vw,30px)] leading-tight font-bold tracking-tight">{title}</h3>
      <p className="mt-3 max-w-[520px] text-[16px] leading-relaxed text-ink-dim">{body}</p>
      <div className="mt-8 flex flex-1 items-end">{children}</div>
    </article>
  );
}

export function Row({ yes, children }: { yes?: boolean; children: ReactNode }) {
  return (
    <li className="flex items-center gap-3 py-2.5 text-[16px] font-medium">
      <span
        className={`grid h-6 w-6 shrink-0 place-items-center rounded-full ${yes ? "bg-signal" : "edge"}`}
        aria-hidden="true"
      >
        {yes ? (
          <svg width="12" height="12" viewBox="0 0 12 12">
            <path d="M2 6.5l2.5 2.5L10 3" stroke="var(--color-signal-ink)" strokeWidth="2.2" fill="none" strokeLinecap="round" />
          </svg>
        ) : (
          <svg width="10" height="10" viewBox="0 0 10 10">
            <path d="M2 2l6 6M8 2L2 8" stroke="var(--color-mute)" strokeWidth="2" strokeLinecap="round" />
          </svg>
        )}
      </span>
      <span className={yes ? "" : "text-mute line-through decoration-2"}>{children}</span>
      <span className="sr-only">{yes ? engine.privacy.usedLabel : engine.privacy.neverLabel}</span>
    </li>
  );
}

/** Typing as bars between two flat, idle stretches. */
export function IdleGraph() {
  const bars = [6, 14, 22, 18, 30, 26, 34, 20, 28, 16, 24, 10];
  return (
    <div className="w-full rounded-[20px] bg-well p-4 edge">
      <svg viewBox="0 0 300 90" className="h-[90px] w-full" aria-hidden="true">
        <path d="M0 80H300" stroke="var(--color-line)" strokeWidth="2" />
        {bars.map((h, i) => (
          <rect key={i} x={96 + i * 9} y={80 - h * 2} width="6" height={h * 2} rx="3" fill="var(--color-signal)" />
        ))}
        <text x="4" y="72" fontSize="11" fill="var(--color-mute)" style={mono}>
          {engine.idle.idle}
        </text>
        <text x="110" y="12" fontSize="11" fill="var(--color-mute)" style={mono}>
          {engine.idle.typing}
        </text>
        <text x="250" y="72" fontSize="11" fill="var(--color-mute)" style={mono}>
          {engine.idle.idle}
        </text>
      </svg>
      <p className="mt-1 font-display text-[32px] leading-none font-bold tracking-tight">{engine.idle.value}</p>
      <p className="mt-1 text-[14px] text-mute">{engine.idle.caption}</p>
    </div>
  );
}

export function PanStrip() {
  /* Four cards to a row (xl) leave about 225px: smaller caps keep all six inside. */
  const cap = (k: string, tone: "cream" | "signal") => (
    <Cap key={k} tone={tone} className="h-10 w-10 [--depth:5px] [--r:10px] xl:h-[30px] xl:w-[30px] xl:[--depth:4px] xl:[--r:8px]">
      <span className="font-display text-[15px] font-bold xl:text-[12px]">{k}</span>
    </Cap>
  );
  return (
    <div className="w-full">
      <div className="flex items-center gap-2">
        <div className="flex gap-1.5 xl:gap-1">{[...engine.panning.leftKeys].map((k) => cap(k, "cream"))}</div>
        <span className="h-0 flex-1 border-t-2 border-dashed border-line" />
        <div className="flex gap-1.5 xl:gap-1">{[...engine.panning.rightKeys].map((k) => cap(k, "signal"))}</div>
      </div>
      <div className="mt-3 flex justify-between font-mono text-[12px] text-mute">
        <span>{engine.panning.left}</span>
        <span>{engine.panning.right}</span>
      </div>
    </div>
  );
}

export function PanelPair() {
  return (
    <div className="grid w-full grid-cols-2 gap-3 xl:gap-2" aria-hidden="true">
      <div className="rounded-[18px] bg-page p-3.5 edge xl:p-2.5">
        <div className="flex items-center justify-between gap-1">
          <span className="min-w-0 truncate font-display text-[15px] font-bold xl:text-[11px]">
            Keyfeel<span className="text-signal">.</span>
          </span>
          <span className="h-4 w-7 shrink-0 rounded-full bg-signal p-0.5 xl:h-3.5 xl:w-6">
            <span className="ml-auto block h-3 w-3 rounded-full bg-signal-ink xl:h-2.5 xl:w-2.5" />
          </span>
        </div>
        <div className="mt-3 h-1.5 rounded-full bg-raised">
          <div className="h-full w-2/3 rounded-full bg-signal" />
        </div>
        <div className="mt-3 space-y-1.5">
          {[70, 50, 60].map((w, i) => (
            <div key={i} className="h-2 rounded-full bg-raised" style={{ width: `${w}%` }} />
          ))}
        </div>
        <p className="mt-3 font-mono text-[11px] text-mute">{engine.panel.graphite}</p>
      </div>
      <div className="relative overflow-hidden rounded-[18px] edge">
        <div className="absolute inset-0 bg-[#2b4f7a]" />
        <div className="absolute -top-6 -left-4 h-20 w-20 rounded-full bg-signal/80" />
        <div className="absolute -right-5 bottom-2 h-16 w-16 rounded-full bg-[#7a5cc2]" />
        <div className="relative m-2 space-y-2">
          {[0, 1, 2].map((i) => (
            <div key={i} className="rounded-[12px] border border-white/30 bg-white/15 p-2 backdrop-blur-md">
              <div className="h-2 rounded-full bg-white/70" style={{ width: `${80 - i * 20}%` }} />
            </div>
          ))}
        </div>
        <p className="relative mx-3.5 mb-3 font-mono text-[11px] text-white/90">{engine.panel.glass}</p>
      </div>
    </div>
  );
}
