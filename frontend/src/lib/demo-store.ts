"use client";

import { useSyncExternalStore } from "react";

import { demoPacks } from "./packs";

/* The demo's selected pack, shared by the demo and the "Hear it" buttons elsewhere. */
let current = demoPacks[0].id;
const listeners = new Set<() => void>();

export function selectDemoPack(id: string) {
  if (id === current || !demoPacks.some((p) => p.id === id)) {
    return;
  }
  current = id;
  listeners.forEach((l) => l());
}

export function useDemoPack() {
  return useSyncExternalStore(
    (l) => {
      listeners.add(l);
      return () => listeners.delete(l);
    },
    () => current,
    () => demoPacks[0].id,
  );
}

/** Fired on every demo keystroke, so the characters can react. */
export const PRESS_EVENT = "keyfeel:press";
