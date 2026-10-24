import type { CSSProperties, ReactNode } from "react";

import { site } from "@/lib/site";
import type { SwitchKind } from "@/lib/packs";

import { lightStem } from "./palette";

/** The mark: a black keycap tile with a white K, and the wordmark with the app's orange dot. */
export function Logo({ size = 40 }: { size?: number }) {
  return (
    <span className="inline-flex items-center gap-2.5">
      <svg width={size} height={size} viewBox="0 0 48 48" aria-hidden="true">
        <rect width="48" height="48" rx="12" fill="var(--color-ink)" />
        <rect x="9" y="8" width="30" height="27" rx="7" fill="var(--color-paper)" />
        <rect x="9" y="31" width="30" height="8" rx="4" fill="var(--color-ink-soft)" />
        <path
          d="M18 14v14M18 21.5l9-7.5M21.5 19l6 9"
          stroke="var(--color-ink)"
          strokeWidth="3.4"
          strokeLinecap="round"
          strokeLinejoin="round"
          fill="none"
        />
      </svg>
      <span className="font-display text-[26px] font-extrabold tracking-tight">
        Keyfeel<span className="text-signal">.</span>
      </span>
    </span>
  );
}

/** A four-point sparkle, the page's punctuation. */
export function Sparkle({
  color,
  size,
  className = "",
  style,
}: {
  color: string;
  size: number;
  className?: string;
  style?: CSSProperties;
}) {
  return (
    <svg width={size} height={size} viewBox="0 0 100 100" aria-hidden="true" className={className} style={style}>
      <path d="M50 0C54 36 64 46 100 50C64 54 54 64 50 100C46 64 36 54 0 50C36 46 46 36 50 0Z" fill={color} />
    </svg>
  );
}

/** A switch stem (the cross a keycap sits on), coloured by switch kind. */
export function Stem({ kind, size = 44 }: { kind: SwitchKind; size?: number }) {
  return (
    <span
      className="inline-grid shrink-0 place-items-center rounded-[10px] border-[2.5px] border-ink bg-white"
      style={{ width: size, height: size }}
      aria-hidden="true"
    >
      <svg width={size * 0.56} height={size * 0.56} viewBox="0 0 24 24">
        <path
          d="M9.5 2.5h5v7h7v5h-7v7h-5v-7h-7v-5h7z"
          fill={lightStem[kind]}
          stroke="var(--color-ink)"
          strokeWidth="1.6"
          strokeLinejoin="round"
        />
      </svg>
    </span>
  );
}

/** Small rounded label above a section heading. */
export function Eyebrow({ children, color = "var(--color-sun)" }: { children: ReactNode; color?: string }) {
  return (
    <span
      className="inline-block rounded-full px-4 py-1.5 text-[13px] font-bold tracking-[0.14em] uppercase"
      style={{ background: color }}
    >
      {children}
    </span>
  );
}

/** A highlighted word or phrase in a heading. */
export function Mark({ children, color }: { children: ReactNode; color?: string }) {
  return (
    <span className="mark" style={color ? ({ "--mark": color } as CSSProperties) : undefined}>
      {children}
    </span>
  );
}

export function ArrowRight({ size = 18 }: { size?: number }) {
  return (
    <svg width={size} height={size} viewBox="0 0 24 24" aria-hidden="true">
      <path d="M5 12h13M13 6l6 6-6 6" stroke="currentColor" strokeWidth="2.6" strokeLinecap="round" strokeLinejoin="round" fill="none" />
    </svg>
  );
}

function DownloadIcon({ size }: { size: number }) {
  return (
    <svg width={size} height={size} viewBox="0 0 24 24" aria-hidden="true">
      <path d="M12 4v11M7 10.5l5 5 5-5M5 20h14" stroke="currentColor" strokeWidth="2.6" strokeLinecap="round" strokeLinejoin="round" fill="none" />
    </svg>
  );
}

/** The DMG download. `tone` picks ink-on-sun or paper-on-ink; `size` the hero or nav scale. */
export function DownloadButton({
  tone = "ink",
  size = "lg",
  label = "Download for Mac",
}: {
  tone?: "ink" | "sun";
  size?: "sm" | "lg";
  label?: string;
}) {
  const tones = {
    ink: "bg-ink text-paper hover:bg-[#222]",
    sun: "bg-sun text-ink hover:bg-[#ffe577]",
  };
  const sizes = {
    sm: "h-11 px-5 text-[15px] gap-2",
    lg: "h-[72px] px-9 text-[21px] gap-3",
  };
  return (
    <a
      href={site.dmgUrl}
      download
      className={`group inline-flex items-center rounded-full font-bold transition-[transform,background-color] duration-150 hover:-translate-y-0.5 active:translate-y-0 ${tones[tone]} ${sizes[size]}`}
    >
      {label}
      <span className="transition-transform duration-150 group-hover:translate-y-0.5">
        <DownloadIcon size={size === "lg" ? 22 : 17} />
      </span>
    </a>
  );
}

export function DownloadMeta({ className = "" }: { className?: string }) {
  return (
    <p className={`text-[14px] text-ink-mute ${className}`}>
      v{site.version} · {site.requirements} · {site.dmgSize}
    </p>
  );
}
