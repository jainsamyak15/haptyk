"use client";

import { type CSSProperties, useRef, useState } from "react";

import { hit } from "@/lib/audio";
import { PRESS_EVENT } from "@/lib/demo-store";

/* The hero's keys, tilted a little like a handful of loose keycaps. */
const KEYS = [
  { label: "C", tilt: -7, lift: 10 },
  { label: "L", tilt: 4, lift: -6 },
  { label: "A", tilt: -3, lift: 14 },
  { label: "C", tilt: 6, lift: -2 },
  { label: "K", tilt: -5, lift: 8, signal: true },
];
const WORDS = ["clack", "thock", "tik!", "clunk", "tok", "click"];

type Pop = { id: number; key: number; word: string; tilt: number };

/** Five pressable keycaps. Each press plays a real recording, panned to where the key sits. */
export function ClackKeys() {
  const [down, setDown] = useState<number | null>(null);
  const [pops, setPops] = useState<Pop[]>([]);
  const next = useRef(0);

  const press = (i: number) => {
    void hit("nk-cream", "key", (i / (KEYS.length - 1)) * 1.2 - 0.6);
    window.dispatchEvent(new Event(PRESS_EVENT));
    setDown(i);
    const id = next.current++;
    const pop = { id, key: i, word: WORDS[id % WORDS.length], tilt: ((id * 37) % 25) - 12 };
    setPops((p) => [...p.slice(-6), pop]);
    window.setTimeout(() => setPops((p) => p.filter((x) => x.id !== id)), 760);
  };

  return (
    <div className="flex items-end justify-center gap-[clamp(8px,1.6vw,22px)]" role="group" aria-label="Keycaps you can press">
      {KEYS.map((k, i) => (
        <div key={i} className="relative" style={{ translate: `0 ${k.lift}px` }}>
          {pops
            .filter((p) => p.key === i)
            .map((p) => (
              <span
                key={p.id}
                aria-hidden="true"
                className="pointer-events-none absolute top-0 left-1/2 z-10 animate-pop rounded-full bg-ink px-3 py-1 font-mono text-[clamp(12px,1.2vw,15px)] font-bold whitespace-nowrap text-page"
                style={{ "--tilt": `${p.tilt}deg` } as CSSProperties}
              >
                {p.word}
              </span>
            ))}
          <button
            type="button"
            aria-label={`Press ${k.label}`}
            data-down={down === i}
            onPointerDown={(e) => {
              e.preventDefault();
              press(i);
            }}
            onPointerUp={() => setDown(null)}
            onPointerLeave={() => setDown((d) => (d === i ? null : d))}
            onKeyDown={(e) => {
              if ((e.key === " " || e.key === "Enter") && !e.repeat) {
                e.preventDefault();
                press(i);
              }
            }}
            onKeyUp={() => setDown(null)}
            className={`cap ${k.signal ? "cap-signal" : ""} h-[clamp(64px,11vw,156px)] w-[clamp(60px,10.4vw,148px)] cursor-pointer [--depth:clamp(6px,0.9vw,12px)] [--r:clamp(12px,1.6vw,22px)]`}
            style={{ rotate: `${k.tilt}deg` }}
          >
            <span className="cap-top font-display text-[clamp(28px,5vw,72px)] font-bold">{k.label}</span>
          </button>
        </div>
      ))}
    </div>
  );
}
