"use client";

import { type KeyboardEvent, useCallback, useEffect, useRef, useState } from "react";

import { loadPack, type PackSounds, playKey, setMuted, type SoundKind, startAudio } from "./audio";
import { PRESS_EVENT, useDemoPack } from "./demo-store";
import { demoPacks } from "./packs";

/* The typing demo's keyboard and behaviour, shared by both designs' demos (which differ only
 * in markup). */

/* A Mac keyboard by physical key (KeyboardEvent.code), so the layout never depends on the
 * visitor's language settings. Widths are in key units; every row adds up to 15. */
export type Key = { code: string; label: string; w?: number; mod?: boolean };

const letters = (s: string) => [...s].map((c) => ({ code: `Key${c}`, label: c }));

export const ROWS: Key[][] = [
  [
    { code: "Backquote", label: "`" },
    ...[..."1234567890"].map((d) => ({ code: `Digit${d}`, label: d })),
    { code: "Minus", label: "-" },
    { code: "Equal", label: "=" },
    { code: "Backspace", label: "delete", w: 2, mod: true },
  ],
  [
    { code: "Tab", label: "tab", w: 1.5, mod: true },
    ...letters("QWERTYUIOP"),
    { code: "BracketLeft", label: "[" },
    { code: "BracketRight", label: "]" },
    { code: "Backslash", label: "\\", w: 1.5 },
  ],
  [
    { code: "CapsLock", label: "caps", w: 1.75, mod: true },
    ...letters("ASDFGHJKL"),
    { code: "Semicolon", label: ";" },
    { code: "Quote", label: "'" },
    { code: "Enter", label: "return", w: 2.25, mod: true },
  ],
  [
    { code: "ShiftLeft", label: "shift", w: 2.25, mod: true },
    ...letters("ZXCVBNM"),
    { code: "Comma", label: "," },
    { code: "Period", label: "." },
    { code: "Slash", label: "/" },
    { code: "ShiftRight", label: "shift", w: 2.75, mod: true },
  ],
  [
    { code: "ControlLeft", label: "control", w: 1.25, mod: true },
    { code: "AltLeft", label: "option", w: 1.25, mod: true },
    { code: "MetaLeft", label: "⌘", w: 1.5, mod: true },
    { code: "Space", label: "", w: 5.5 },
    { code: "MetaRight", label: "⌘", w: 1.5, mod: true },
    { code: "AltRight", label: "option", w: 1.25, mod: true },
    { code: "ArrowLeft", label: "←", w: 1.375 },
    { code: "ArrowRight", label: "→", w: 1.375 },
  ],
];

/* Each key's pan from its centre across the row, as the app pans keys left to right. */
const PAN = new Map<string, number>();
for (const row of ROWS) {
  let x = 0;
  for (const k of row) {
    const w = k.w ?? 1;
    PAN.set(k.code, ((x + w / 2) / 15) * 2 - 1);
    x += w;
  }
}

const QUIET = new Set([
  "Tab",
  "CapsLock",
  "ShiftLeft",
  "ShiftRight",
  "ControlLeft",
  "ControlRight",
  "AltLeft",
  "AltRight",
  "MetaLeft",
  "MetaRight",
]);

function soundFor(code: string): SoundKind {
  if (code === "Space") {
    return "space";
  }
  if (code === "Enter" || code === "NumpadEnter") {
    return "enter";
  }
  if (code === "Backspace" || code === "Delete") {
    return "backspace";
  }
  return "key";
}

/* Keys whose browser default would scroll the page or open find-in-page. */
const SWALLOW = new Set(["Space", "ArrowLeft", "ArrowRight", "ArrowUp", "ArrowDown", "Quote", "Slash", "Backspace"]);


export const TAP_PAD = [
  { code: "KeyK", label: "Key" },
  { code: "Space", label: "Space" },
  { code: "Enter", label: "Return" },
  { code: "Backspace", label: "Delete" },
];

/** State and handlers for one demo: put `pad` and the handlers on the focusable element that
 * listens for keys. */
export function useTypingDemo() {
  const packId = useDemoPack();
  const pack = demoPacks.find((p) => p.id === packId) ?? demoPacks[0];

  const [started, setStarted] = useState(false);
  const [focused, setFocused] = useState(false);
  const [muted, setMutedState] = useState(false);
  const [down, setDown] = useState<ReadonlySet<string>>(new Set());
  const [text, setText] = useState("");
  const [ready, setReady] = useState<string | null>(null);

  const sounds = useRef<{ id: string; sounds: PackSounds } | null>(null);
  const current = useRef(packId);
  const mutedRef = useRef(false);
  const pad = useRef<HTMLDivElement>(null);

  useEffect(() => {
    current.current = packId;
    if (!started) {
      return;
    }
    let live = true;
    loadPack(packId)
      .then((s) => {
        if (live && current.current === packId) {
          sounds.current = { id: packId, sounds: s };
          setReady(packId);
        }
      })
      .catch(() => {
        if (live) {
          setReady(null);
        }
      });
    return () => {
      live = false;
    };
  }, [packId, started]);

  const start = useCallback(() => {
    startAudio();
    setStarted(true);
  }, []);

  const press = useCallback((code: string, char?: string) => {
    const s = sounds.current;
    if (s && s.id === current.current && !mutedRef.current) {
      playKey(s.id, s.sounds, soundFor(code), (PAN.get(code) ?? 0) * 0.7, QUIET.has(code));
    }
    window.dispatchEvent(new Event(PRESS_EVENT));
    setDown((d) => new Set(d).add(code));
    if (code === "Backspace") {
      setText((t) => t.slice(0, -1));
    } else if (code === "Enter") {
      setText((t) => `${t} ↵ `.slice(-120));
    } else if (char) {
      setText((t) => (t + char).slice(-120));
    }
  }, []);

  const release = useCallback((code: string) => {
    setDown((d) => {
      if (!d.has(code)) {
        return d;
      }
      const next = new Set(d);
      next.delete(code);
      return next;
    });
  }, []);

  const onKeyDown = (e: KeyboardEvent<HTMLDivElement>) => {
    const isModKey = QUIET.has(e.code);
    if ((e.metaKey || e.ctrlKey) && !isModKey) {
      return; // Leave shortcuts such as ⌘R alone.
    }
    if (SWALLOW.has(e.code)) {
      e.preventDefault();
    }
    if (e.repeat) {
      return; // Held keys repeat silently, as in the app.
    }
    start();
    press(e.code, e.key.length === 1 ? e.key : undefined);
  };

  const toggleMute = () => {
    const next = !muted;
    mutedRef.current = next;
    setMutedState(next);
    setMuted(next);
  };

  const listening = focused && started;
  const loading = started && ready !== packId;

  return {
    packId,
    pack,
    started,
    focused,
    setFocused,
    muted,
    down,
    setDown,
    text,
    listening,
    loading,
    pad,
    start,
    press,
    release,
    onKeyDown,
    toggleMute,
  };
}
