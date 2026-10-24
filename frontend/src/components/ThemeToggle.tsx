"use client";

import { useSyncExternalStore } from "react";

import { THEME_KEY } from "@/lib/theme";

/* The page follows the system appearance until the visitor picks one here. The choice is a
 * per-visitor convenience in localStorage; layout.tsx reapplies it before first paint. */
const EVENT = "keyfeel:theme";

type Theme = "light" | "dark";

function current(): Theme {
  const picked = document.documentElement.dataset.theme;
  if (picked === "light" || picked === "dark") {
    return picked;
  }
  return window.matchMedia("(prefers-color-scheme: light)").matches ? "light" : "dark";
}

function subscribe(onChange: () => void) {
  const media = window.matchMedia("(prefers-color-scheme: light)");
  media.addEventListener("change", onChange);
  window.addEventListener(EVENT, onChange);
  return () => {
    media.removeEventListener("change", onChange);
    window.removeEventListener(EVENT, onChange);
  };
}

export function ThemeToggle({ className = "" }: { className?: string }) {
  const theme = useSyncExternalStore(subscribe, current, () => null);
  const next: Theme = theme === "light" ? "dark" : "light";
  return (
    <button
      type="button"
      onClick={() => {
        document.documentElement.dataset.theme = next;
        try {
          localStorage.setItem(THEME_KEY, next);
        } catch {
          // Private windows may refuse storage; the switch still works for this visit.
        }
        window.dispatchEvent(new Event(EVENT));
      }}
      aria-label={theme ? `Switch to ${next} mode` : "Switch appearance"}
      title={theme ? `Switch to ${next} mode` : undefined}
      className={`grid h-11 w-11 shrink-0 place-items-center rounded-full transition-colors ${className}`}
    >
      <svg width="20" height="20" viewBox="0 0 24 24" aria-hidden="true">
        {theme === "light" ? (
          <path d="M20 14.5A8 8 0 0 1 9.5 4a8 8 0 1 0 10.5 10.5z" fill="currentColor" />
        ) : (
          <g stroke="currentColor" strokeWidth="2.2" strokeLinecap="round" fill="none">
            <circle cx="12" cy="12" r="4.2" fill="currentColor" stroke="none" />
            <path d="M12 2.5v2.2M12 19.3v2.2M2.5 12h2.2M19.3 12h2.2M5.3 5.3l1.6 1.6M17.1 17.1l1.6 1.6M5.3 18.7l1.6-1.6M17.1 6.9l1.6-1.6" />
          </g>
        )}
      </svg>
    </button>
  );
}
