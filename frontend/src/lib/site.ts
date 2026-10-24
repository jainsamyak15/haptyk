/**
 * Facts the page shows, in one place. Numbers come from docs/PERFORMANCE.md and the build
 * config; keep them in step with the app.
 */
export const site = {
  name: "Keyfeel",
  version: "0.1.0",
  /** Where "Download" points. Set NEXT_PUBLIC_DMG_URL to override (for example a CDN). The
   * default is the DMG committed at public/downloads/Keyfeel.dmg and served by this site, so a
   * release replaces that file under exactly this name (docs/RELEASING.md). */
  dmgUrl: process.env.NEXT_PUBLIC_DMG_URL ?? "/downloads/Keyfeel.dmg",
  dmgSize: "25 MB",
  requirements: "macOS 13 or later · Apple Silicon",
  packCount: 25,
} as const;
