import { download, nav } from "@/lib/content";

import { ThemeToggle } from "../ThemeToggle";
import { DownloadButton, Logo } from "./Bits";

export function LightNav() {
  return (
    <header className="sticky top-0 z-50 border-b-2 border-ink bg-paper/90 backdrop-blur-md">
      <nav className="mx-auto flex h-[84px] max-w-[1320px] items-center justify-between gap-6 px-5 sm:px-8">
        <div className="flex items-center gap-9">
          <a href="#l-top" aria-label={nav.home}>
            <Logo />
          </a>
          <ul className="hidden items-center gap-8 text-[17px] font-medium md:flex">
            {nav.links.map((l) => (
              <li key={l.to}>
                <a href={`#l-${l.to}`} className="underline-offset-[6px] hover:underline">
                  {l.label}
                </a>
              </li>
            ))}
          </ul>
        </div>
        <div className="flex items-center gap-3">
          <ThemeToggle className="border-2 border-ink bg-white hover:bg-sun" />
          <a
            href="#l-try"
            className="hidden h-11 items-center rounded-full bg-stone px-5 text-[15px] font-bold transition-colors hover:bg-[#d8d3c6] sm:inline-flex"
          >
            {nav.tryIt}
          </a>
          <DownloadButton size="sm" label={download.short} />
        </div>
      </nav>
    </header>
  );
}
