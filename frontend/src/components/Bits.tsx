import type { CSSProperties, ReactNode } from "react";

import { site } from "@/lib/site";
import { type SwitchKind, stemColor } from "@/lib/packs";

/** The mark: a cream keycap with a K, and the app's wordmark with its orange dot. */
export function Logo({ size = 38 }: { size?: number }) {
  return (
    <span className="inline-flex items-center gap-2.5">
      <svg width={size} height={size} viewBox="0 0 48 48" aria-hidden="true">
        <rect width="48" height="48" rx="12" fill="var(--color-cap-side)" />
        <rect x="4" y="3" width="40" height="36" rx="9" fill="var(--color-cap)" />
        <path
          d="M17 11v19M17 21.5l11-10.5M21.5 18l7.5 12"
          stroke="var(--color-cap-ink)"
          strokeWidth="3.8"
          strokeLinecap="round"
          strokeLinejoin="round"
          fill="none"
        />
        <circle cx="37" cy="31" r="3.2" fill="var(--color-signal)" />
      </svg>
      <span className="font-display text-[21px] font-bold tracking-tight">
        Keyfeel<span className="text-signal">.</span>
      </span>
    </span>
  );
}

/** A switch stem from above: the cross a keycap sits on, coloured by switch kind. */
export function Stem({ kind, size = 40 }: { kind: SwitchKind; size?: number }) {
  return (
    <span
      className="inline-grid shrink-0 place-items-center rounded-[10px] bg-raised edge"
      style={{ width: size, height: size }}
      aria-hidden="true"
    >
      <svg width={size * 0.56} height={size * 0.56} viewBox="0 0 24 24">
        <path d="M9.5 2.5h5v7h7v5h-7v7h-5v-7h-7v-5h7z" fill={stemColor[kind]} />
      </svg>
    </span>
  );
}

/** A static keycap. Give it a size with `className` (width and height). */
export function Cap({
  children,
  tone = "cream",
  className = "",
  style,
}: {
  children?: ReactNode;
  tone?: "cream" | "signal" | "mod";
  className?: string;
  style?: CSSProperties;
}) {
  const toneClass = tone === "signal" ? "cap-signal" : tone === "mod" ? "cap-mod" : "";
  return (
    <span className={`cap ${toneClass} ${className}`} style={style} aria-hidden="true">
      <span className="cap-top">{children}</span>
    </span>
  );
}

/** "01 / Switches": the small numbered label above each section. */
export function Overline({ n, children }: { n: string; children: ReactNode }) {
  return (
    <p className="font-mono text-[13px] font-medium tracking-[0.18em] text-mute uppercase">
      <span className="text-signal">{n}</span> / {children}
    </p>
  );
}

export function ArrowRight({ size = 18 }: { size?: number }) {
  return (
    <svg width={size} height={size} viewBox="0 0 24 24" aria-hidden="true">
      <path
        d="M5 12h13M13 6l6 6-6 6"
        stroke="currentColor"
        strokeWidth="2.4"
        strokeLinecap="round"
        strokeLinejoin="round"
        fill="none"
      />
    </svg>
  );
}

function DownloadIcon({ size }: { size: number }) {
  return (
    <svg width={size} height={size} viewBox="0 0 24 24" aria-hidden="true">
      <path
        d="M12 4v11M7 10.5l5 5 5-5M5 20h14"
        stroke="currentColor"
        strokeWidth="2.6"
        strokeLinecap="round"
        strokeLinejoin="round"
        fill="none"
      />
    </svg>
  );
}

/**
 * The DMG download, shaped like the orange Escape keycap: it presses down while held.
 * `size` picks the hero or the nav scale.
 */
export function DownloadButton({ size = "lg", label = "Download for Mac" }: { size?: "sm" | "lg"; label?: string }) {
  const big = size === "lg";
  return (
    <a
      href={site.dmgUrl}
      download
      className={`cap cap-signal group inline-block font-bold active:[&>.cap-top]:[transform:translateY(calc(var(--depth)-3px))] ${
        big ? "h-[78px] [--depth:10px] [--r:20px]" : "h-[46px] [--depth:6px] [--r:13px]"
      }`}
    >
      <span className={`cap-top flex! gap-2.5 whitespace-nowrap ${big ? "px-8 text-[19px]" : "px-4 text-[14px]"}`}>
        {label}
        <DownloadIcon size={big ? 21 : 16} />
      </span>
      <span className={`invisible flex gap-2.5 whitespace-nowrap ${big ? "px-9 text-[19px]" : "px-5 text-[14px]"}`}>
        {label}
        <DownloadIcon size={big ? 21 : 16} />
      </span>
    </a>
  );
}

export function DownloadMeta({ className = "" }: { className?: string }) {
  return (
    <p className={`font-mono text-[13px] text-mute ${className}`}>
      v{site.version} · {site.requirements} · {site.dmgSize}
    </p>
  );
}
