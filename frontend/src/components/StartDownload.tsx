"use client";

import { useEffect } from "react";

import { site } from "@/lib/site";

/* React runs effects twice in development: remember the last start so that is one download, while
 * a later visit to /download still starts a new one. */
let lastStart = 0;

/** Starts the DMG download as soon as /download shows. Renders nothing. */
export function StartDownload() {
  useEffect(() => {
    const now = Date.now();
    if (now - lastStart < 3000) {
      return;
    }
    lastStart = now;
    const link = document.createElement("a");
    link.href = site.dmgUrl;
    link.download = "Keyfeel.dmg";
    document.body.append(link);
    link.click();
    link.remove();
  }, []);
  return null;
}
