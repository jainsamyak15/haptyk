import { packBand } from "@/lib/content";
import { packs } from "@/lib/packs";

import { Stem } from "./Bits";

/** Every bundled pack, scrolling past on a graphite band. Pauses on hover; still under Reduced Motion. */
export function PackBand() {
  return (
    <section aria-label={packBand.label} className="overflow-hidden rule-t rule-b bg-surface">
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
                <span className="font-display text-[clamp(26px,3vw,40px)] font-bold tracking-[-0.03em] whitespace-nowrap">
                  {p.name}
                </span>
                <span className="mx-6 h-2.5 w-2.5 rounded-full bg-signal" />
              </span>
            ))}
          </div>
        ))}
      </div>
    </section>
  );
}
