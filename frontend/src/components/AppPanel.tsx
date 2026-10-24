import { stemColor } from "@/lib/packs";

import { Overline } from "./Bits";

const POINTS = [
  ["One click away", "Switch packs, set the volume and see today's typing from the menu bar."],
  ["Graphite or Glass", "Warm graphite like this, or Apple-style glass over your desktop."],
  ["Knows when to hush", "Goes quiet while an app uses your microphone, and pauses while your Mac sleeps."],
  ["Still a menu", "Right-click the icon for the classic menu with every command."],
] as const;

const ROWS = [
  { name: "Holy Panda", kind: "tactile" as const, on: false },
  { name: "NK Cream", kind: "linear" as const, on: true },
  { name: "Cherry MX Blue ABS", kind: "clicky" as const, on: false },
  { name: "Topre PBT", kind: "other" as const, on: false },
];

/** The menu bar panel, drawn in the app's Graphite & Signal colours. An illustration. */
function PanelDrawing() {
  return (
    <div
      aria-hidden="true"
      className="mx-auto w-full max-w-[380px] rounded-[26px] bg-[#121211] p-5 text-[#f2efe9] shadow-[0_40px_80px_-30px_rgba(0,0,0,0.9)] ring-1 ring-[#2e2c29]"
    >
      <div className="flex items-start justify-between">
        <div>
          <p className="font-display text-[19px] font-bold">
            Keyfeel<span className="text-signal">.</span>
          </p>
          <p className="text-[13px] text-[#8c877e]">NK Cream</p>
        </div>
        <span className="flex h-7 w-12 items-center justify-end rounded-full bg-signal p-1">
          <span className="h-5 w-5 rounded-full bg-[#1b0f05]" />
        </span>
      </div>

      <div className="mt-5 flex items-center gap-3">
        <svg width="14" height="14" viewBox="0 0 24 24">
          <path d="M4 9h4l5-4v14l-5-4H4z" fill="#8c877e" />
        </svg>
        <div className="relative h-1.5 flex-1 rounded-full bg-[#302e2b]">
          <div className="absolute inset-y-0 left-0 w-[68%] rounded-full bg-signal" />
          <div className="absolute top-1/2 left-[68%] h-4 w-4 -translate-x-1/2 -translate-y-1/2 rounded-full border-[3px] border-signal bg-[#121211]" />
        </div>
      </div>

      <p className="mt-6 text-[11px] font-semibold tracking-[0.14em] text-[#8c877e] uppercase">Sound pack</p>
      <div className="mt-3 flex flex-wrap gap-1.5 text-[12px] font-medium">
        {["All", "Linear", "Tactile", "Clicky", "Other"].map((c, i) => (
          <span
            key={c}
            className={`rounded-full px-3 py-1 ${i === 0 ? "bg-signal text-[#1b0f05]" : "ring-1 ring-[#2e2c29] text-[#b7b2a8]"}`}
          >
            {c}
          </span>
        ))}
      </div>
      <div className="mt-3 rounded-[16px] p-1.5 ring-1 ring-[#2e2c29]">
        {ROWS.map((r) => (
          <div
            key={r.name}
            className={`flex items-center justify-between rounded-[11px] px-3 py-2 text-[14px] ${r.on ? "bg-[#1c1b19]" : ""}`}
          >
            <span className={r.on ? "font-semibold" : "text-[#b7b2a8]"}>{r.name}</span>
            <span className="flex items-center gap-2 text-[12px] text-[#8c877e]">
              <span className="h-2 w-2 rounded-full" style={{ background: stemColor[r.kind] }} />
              {r.on && <span className="text-signal">✓</span>}
            </span>
          </div>
        ))}
      </div>

      <div className="mt-4 grid grid-cols-3 gap-2">
        {[
          ["Keys today", "4,812"],
          ["Taps / min", "312"],
          ["Active", "2 h"],
        ].map(([label, value]) => (
          <div key={label} className="rounded-[12px] px-3 py-2.5 ring-1 ring-[#2e2c29]">
            <p className="text-[11px] text-[#8c877e]">{label}</p>
            <p className="mt-0.5 font-display text-[17px] font-bold">{value}</p>
          </div>
        ))}
      </div>
    </div>
  );
}

export function AppPanel() {
  return (
    <section className="rule-t py-28 sm:py-32">
      <div className="mx-auto grid max-w-[1280px] items-center gap-16 px-5 sm:px-8 lg:grid-cols-[minmax(0,1fr)_minmax(0,1fr)]">
        <div>
          <Overline n="03">The app</Overline>
          <h2 className="mt-5 font-display text-[clamp(30px,3.4vw,46px)] leading-[1.08] font-bold tracking-[-0.03em]">
            Lives in your menu bar. Stays out of your way.
          </h2>
          <dl className="mt-10 grid gap-x-8 gap-y-7 sm:grid-cols-2">
            {POINTS.map(([title, body]) => (
              <div key={title}>
                <dt className="font-display text-[16px] font-bold">{title}</dt>
                <dd className="mt-1.5 text-[16px] leading-relaxed text-ink-dim">{body}</dd>
              </div>
            ))}
          </dl>
        </div>
        <PanelDrawing />
      </div>
    </section>
  );
}
