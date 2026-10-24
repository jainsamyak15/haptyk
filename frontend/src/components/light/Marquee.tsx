import { packBand } from "@/lib/content";
import { packs } from "@/lib/packs";

import { Sparkle, Stem } from "./Bits";

/** Every bundled pack, scrolling past on a yellow band. Pauses on hover; still under Reduced Motion. */
export function Marquee() {
  return (
    <section aria-label={packBand.label} className="overflow-hidden border-y-2 border-ink bg-sun">
      <ul className="sr-only">
        {packs.map((p) => (
          <li key={p.name}>{p.name}</li>
        ))}
      </ul>
      <div aria-hidden="true" className="flex w-max animate-marquee py-[18px] hover:[animation-play-state:paused]">
        {[0, 1].map((copy) => (
          <div key={copy} className="flex shrink-0 items-center">
            {packs.map((p) => (
              <span key={p.name} className="flex items-center gap-4 pl-4">
                <Stem kind={p.kind} />
                <span className="font-display text-[clamp(30px,3.4vw,46px)] font-extrabold tracking-[-0.03em] whitespace-nowrap">
                  {p.name}
                </span>
                <Sparkle color="#8a7618" size={26} className="mx-6" />
              </span>
            ))}
          </div>
        ))}
      </div>
    </section>
  );
}
