import { hero } from "@/lib/content";

import { DownloadButton, DownloadMeta } from "./Bits";
import { ClackKeys } from "./ClackKeys";

export function Hero() {
  return (
    <section id="top" className="relative overflow-hidden">
      {/* A faint key-grid, like the plate under the keycaps. */}
      <div
        aria-hidden="true"
        className="pointer-events-none absolute inset-0 [background-image:linear-gradient(var(--color-grid)_1px,transparent_1px),linear-gradient(90deg,var(--color-grid)_1px,transparent_1px)] [background-size:64px_64px]"
      />
      <div className="relative mx-auto flex min-h-[calc(100svh-76px)] max-w-[1280px] flex-col items-center justify-center px-5 pt-20 pb-24 text-center sm:px-8">
        <ClackKeys />

        <h1 className="mt-16 max-w-[1000px] font-display text-[clamp(34px,5.6vw,76px)] leading-[1.04] font-bold tracking-[-0.03em]">
          {hero.lead} <span className="font-normal text-ink-dim">{hero.soft}</span>{" "}
          <span className="text-signal">{hero.mark}</span>
        </h1>
        <p className="mt-7 max-w-[660px] text-[clamp(17px,1.5vw,20px)] leading-relaxed text-ink-dim">{hero.body}</p>
        <div className="mt-11 flex flex-col items-center gap-4">
          <DownloadButton />
          <DownloadMeta />
          <a
            href="#try"
            className="mt-1 text-[16px] font-semibold text-ink-dim underline decoration-line decoration-2 underline-offset-[6px] transition-colors hover:text-ink hover:decoration-signal"
          >
            {hero.tryLink}
          </a>
        </div>
      </div>
    </section>
  );
}
