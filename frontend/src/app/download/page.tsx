import type { Metadata } from "next";

import { Install } from "@/components/Install";
import { LightInstall } from "@/components/light/Install";
import { StartDownload } from "@/components/StartDownload";

export const metadata: Metadata = {
  title: "Download Keyfeel",
  description: "Your download has started. Here's how to open Keyfeel on your Mac.",
};

/* Every Download button leads here: the DMG starts downloading and the page shows how to open it,
 * in whichever design is showing (globals.css shows exactly one). */
export default function DownloadPage() {
  return (
    <>
      <StartDownload />
      <div className="design-dark">
        <Install />
      </div>
      <div className="design-light">
        <LightInstall />
      </div>
    </>
  );
}
