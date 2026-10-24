import type { ReactNode } from "react";

import { engine } from "@/lib/content";

/* The pieces of the light design's "under the hood" cards. */

const mono = { fontFamily: "var(--font-mono)" };

export function Card({
  n,
  title,
  body,
  color,
  className = "",
  children,
}: {
  n: string;
  title: string;
  body: ReactNode;
  color: string;
  className?: string;
  children: ReactNode;
}) {
  return (
    <article className={`flex flex-col overflow-hidden rounded-[30px] p-7 text-[#0b0b0b] sm:p-8 ${className}`} style={{ background: color }}>
      <span className="font-mono text-[13px] font-semibold opacity-70">{n}</span>
      <h3 className="mt-2 font-display text-[clamp(28px,2.6vw,36px)] leading-tight font-extrabold tracking-tight">{title}</h3>
      <p className="mt-3 max-w-[520px] text-[17px] leading-relaxed opacity-80">{body}</p>
      <div className="mt-8 flex flex-1 items-end">{children}</div>
    </article>
  );
}

export function Row({ yes, children }: { yes?: boolean; children: ReactNode }) {
  return (
    <li className="flex items-center gap-3 py-2.5 text-[17px] font-medium">
      <span
        className={`grid h-6 w-6 shrink-0 place-items-center rounded-full border-2 border-ink ${yes ? "bg-leaf" : "bg-white"}`}
        aria-hidden="true"
      >
        {yes ? (
          <svg width="12" height="12" viewBox="0 0 12 12">
            <path d="M2 6.5l2.5 2.5L10 3" stroke="#fff" strokeWidth="2.2" fill="none" strokeLinecap="round" />
          </svg>
        ) : (
          <svg width="10" height="10" viewBox="0 0 10 10">
            <path d="M2 2l6 6M8 2L2 8" stroke="var(--color-ink)" strokeWidth="2" strokeLinecap="round" />
          </svg>
        )}
      </span>
      <span className={yes ? "" : "line-through decoration-2 opacity-70"}>{children}</span>
      <span className="sr-only">{yes ? engine.privacy.usedLabel : engine.privacy.neverLabel}</span>
    </li>
  );
}

/** Typing as bars between two flat, idle stretches. */
export function IdleGraph() {
  const bars = [6, 14, 22, 18, 30, 26, 34, 20, 28, 16, 24, 10];
  return (
    <div className="w-full rounded-[20px] border-2 border-ink bg-white p-4">
      <svg viewBox="0 0 300 90" className="h-[90px] w-full" aria-hidden="true">
        <path d="M0 80H300" stroke="var(--color-ink)" strokeWidth="2" />
        {bars.map((h, i) => (
          <rect key={i} x={96 + i * 9} y={80 - h * 2} width="6" height={h * 2} rx="3" fill="var(--color-ink)" />
        ))}
        <text x="4" y="72" fontSize="11" fill="var(--color-ink)" opacity="0.6" style={mono}>
          {engine.idle.idle}
        </text>
        <text x="110" y="12" fontSize="11" fill="var(--color-ink)" opacity="0.6" style={mono}>
          {engine.idle.typing}
        </text>
        <text x="250" y="72" fontSize="11" fill="var(--color-ink)" opacity="0.6" style={mono}>
          {engine.idle.idle}
        </text>
      </svg>
      <p className="mt-1 font-display text-[32px] leading-none font-extrabold tracking-tight">{engine.idle.value}</p>
      <p className="mt-1 text-[14px] font-semibold text-ink-soft">{engine.idle.caption}</p>
    </div>
  );
}

export function PanStrip() {
  /* Four cards to a row (xl) leave about 225px: smaller caps keep all six inside. */
  const cap = (k: string, color: string) => (
    <span
      key={k}
      className="keycap grid h-10 w-10 place-items-center rounded-[10px] font-display text-[16px] font-bold [--depth:4px] xl:h-[30px] xl:w-[30px] xl:rounded-[8px] xl:text-[12px] xl:[--depth:3px]"
      style={{ background: color }}
    >
      {k}
    </span>
  );
  return (
    <div className="w-full">
      <div className="flex items-center gap-2">
        <div className="flex gap-1.5 xl:gap-1">{[...engine.panning.leftKeys].map((k) => cap(k, "var(--color-sun)"))}</div>
        <span className="h-0 flex-1 border-t-2 border-dashed border-ink" />
        <div className="flex gap-1.5 xl:gap-1">{[...engine.panning.rightKeys].map((k) => cap(k, "var(--color-tang)"))}</div>
      </div>
      <div className="mt-3 flex justify-between font-mono text-[13px] font-bold">
        <span>{engine.panning.left}</span>
        <span>{engine.panning.right}</span>
      </div>
    </div>
  );
}

export function PanelPair() {
  return (
    <div className="grid w-full grid-cols-2 gap-3" aria-hidden="true">
      <div className="rounded-[18px] border-2 border-ink bg-[#121211] p-3.5 text-[#f2efe9] xl:p-3">
        <div className="flex items-center justify-between gap-1">
          <span className="min-w-0 truncate font-display text-[15px] font-bold xl:text-[12px]">
            Keyfeel<span className="text-signal">.</span>
          </span>
          <span className="h-4 w-7 shrink-0 rounded-full bg-signal p-0.5 xl:h-3.5 xl:w-6">
            <span className="ml-auto block h-3 w-3 rounded-full bg-[#1b0f05] xl:h-2.5 xl:w-2.5" />
          </span>
        </div>
        <div className="mt-3 h-1.5 rounded-full bg-[#302e2b]">
          <div className="h-full w-2/3 rounded-full bg-signal" />
        </div>
        <div className="mt-3 space-y-1.5">
          {[70, 50, 60].map((w, i) => (
            <div key={i} className="h-2 rounded-full bg-[#2e2c29]" style={{ width: `${w}%` }} />
          ))}
        </div>
        <p className="mt-3 font-mono text-[11px] opacity-60">{engine.panel.graphite}</p>
      </div>
      <div className="relative overflow-hidden rounded-[18px] border-2 border-ink">
        <div className="absolute inset-0 bg-[#4b8fd9]" />
        <div className="absolute -top-6 -left-4 h-20 w-20 rounded-full bg-sun" />
        <div className="absolute -right-5 bottom-2 h-16 w-16 rounded-full bg-tang" />
        <div className="relative m-2 space-y-2">
          {[0, 1, 2].map((i) => (
            <div key={i} className="rounded-[12px] border border-white/50 bg-white/25 p-2 backdrop-blur-md">
              <div className="h-2 rounded-full bg-white/80" style={{ width: `${80 - i * 20}%` }} />
            </div>
          ))}
        </div>
        <p className="relative mx-3.5 mb-3 font-mono text-[11px] text-white">{engine.panel.glass}</p>
      </div>
    </div>
  );
}
