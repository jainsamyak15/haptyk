/**
 * Facts the page shows, in one place. Numbers come from docs/PERFORMANCE.md and the build
 * config; keep them in step with the app.
 */
export const site = {
  name: "Keyfeel",
  version: "0.1.0",
  /** Where "Download" points. Set NEXT_PUBLIC_DMG_URL to override (for example a local
   * /downloads/Keyfeel.dmg, or a CDN). The default is the newest GitHub release asset, so a
   * release needs a DMG attached under exactly this file name. */
  dmgUrl:
    process.env.NEXT_PUBLIC_DMG_URL ??
    "https://github.com/jainsamyak15/haptyk/releases/latest/download/Keyfeel.dmg",
  dmgSize: "21 MB",
  requirements: "macOS 13 or later · Apple Silicon",
  packCount: 25,
} as const;
