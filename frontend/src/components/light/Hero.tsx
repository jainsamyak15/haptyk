import { hero } from "@/lib/content";

import { Buddy } from "./Buddy";
import { DownloadButton, DownloadMeta, Mark, Sparkle } from "./Bits";

/** A sky tile with a keycap that keeps tapping itself, sized in em to match the headline. */
function TapTile({ className }: { className: string }) {
  return (
    <span
      aria-hidden="true"
      className={`relative h-[0.9em] w-[0.9em] shrink-0 place-items-center rounded-[0.2em] border-[0.045em] border-ink bg-[#7cc2f2] ${className}`}
    >
      {/* Sizes in this span's own em (its font is 0.32 of the headline's): 1.75em = 0.56 of the
          headline, so the key fills the tile the same way at every size. */}
      <span className="grid h-[1.75em] w-[1.75em] animate-tap place-items-center rounded-[0.4em] border-[0.12em] border-ink bg-white font-display text-[0.32em] leading-none font-extrabold">
        K
      </span>
    </span>
  );
}

export function LightHero() {
  return (
    <section id="l-top" className="relative overflow-hidden">
      <Buddy
        shape="keycap"
        color="var(--color-volt)"
        size={250}
        rotate={-26}
        look={[0.7, 0.5]}
        blinkDelay={1.2}
        className="top-[1%] -left-10 origin-top-left scale-[0.45] sm:scale-[0.65] xl:scale-[0.85] 2xl:scale-100"
      />
      <Buddy
        shape="esc"
        color="var(--color-ink)"
        size={300}
        rotate={16}
        look={[0.6, -0.6]}
        blinkDelay={3.4}
        floatDelay={1.5}
        className="-bottom-28 -left-20 origin-bottom-left scale-50 sm:scale-75 lg:scale-100"
      />
      <Buddy
        shape="pill"
        color="var(--color-leaf)"
        size={200}
        rotate={22}
        look={[-0.7, -0.3]}
        blinkDelay={2.1}
        floatDelay={0.8}
        className="-right-12 bottom-[6%] origin-bottom-right scale-50 sm:scale-75 lg:scale-100"
      />
      <Sparkle color="var(--color-cherry)" size={78} className="absolute top-[24%] -left-4 animate-twinkle" />
      <Sparkle
        color="var(--color-maroon)"
        size={68}
        className="absolute top-[10%] right-[8%] animate-twinkle"
        style={{ animationDelay: "0.8s" }}
      />
      <Sparkle
        color="var(--color-volt)"
        size={56}
        className="absolute top-[54%] right-[9%] hidden animate-twinkle sm:block"
        style={{ animationDelay: "1.6s" }}
      />
      <Sparkle
        color="#f3a43b"
        size={62}
        className="absolute right-[18%] bottom-[10%] hidden animate-twinkle md:block"
        style={{ animationDelay: "2.2s" }}
      />
      <Sparkle
        color="var(--color-volt)"
        size={50}
        className="absolute bottom-[20%] left-[17%] hidden animate-twinkle lg:block"
        style={{ animationDelay: "1.1s" }}
      />

      <div className="relative mx-auto flex min-h-[calc(100svh-84px)] max-w-[1160px] flex-col items-center justify-center px-5 py-28 text-center">
        <h1 className="font-display text-[clamp(44px,7.4vw,100px)] leading-[0.98] tracking-[-0.035em]">
          <span className="md:whitespace-nowrap">
            <span className="font-extrabold">{hero.lead}</span> <span className="font-light">{hero.soft}</span>
            {/* Phones: the tile rides along with "with a" instead of taking a line alone. */}
            <TapTile className="ml-[0.2em] inline-grid align-[-0.1em] sm:hidden" />
          </span>
          <span className="mt-[0.12em] flex flex-wrap items-center justify-center gap-x-[0.22em] gap-y-[0.1em]">
            <TapTile className="hidden sm:inline-grid" />
            <Mark>
              <span className="font-extrabold">{hero.mark}</span>
            </Mark>
          </span>
        </h1>
        <p className="mt-9 max-w-[660px] text-[clamp(18px,1.6vw,21px)] leading-relaxed text-ink-soft">
          {hero.body}
        </p>
        <div className="mt-11 flex flex-col items-center gap-4">
          <DownloadButton />
          <DownloadMeta />
          <a
            href="#l-try"
            className="mt-1 text-[16px] font-semibold underline decoration-2 underline-offset-[6px] hover:decoration-signal"
          >
            {hero.tryLink}
          </a>
        </div>
      </div>
    </section>
  );
}
