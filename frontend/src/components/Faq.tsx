import { faq } from "@/lib/content";
import { FAQ } from "@/lib/faq";

import { Cap } from "./Bits";

export function Faq() {
  return (
    <section id="faq" className="rule-t py-28 sm:py-32">
      <div className="mx-auto max-w-[920px] px-5 sm:px-8">
        <h2 className="font-display text-[clamp(32px,4.6vw,60px)] leading-[1.05] font-bold tracking-[-0.03em]">
          {faq.title.before}
          <span className="text-signal">{faq.title.mark}</span>
        </h2>
        <div className="mt-12 space-y-3">
          {FAQ.map((item, i) => (
            <details key={item.q} className="faq group rounded-[22px] bg-surface edge open:[--color-line:var(--color-signal)]" open={i === 0}>
              <summary className="faq-q flex cursor-pointer items-center gap-4 p-4 pr-6 sm:p-5 sm:pr-7">
                <Cap tone="mod" className="h-11 w-11 shrink-0 [--depth:5px] [--r:11px]">
                  <span className="font-display text-[17px] font-bold">?</span>
                </Cap>
                <span className="flex-1 text-[clamp(16px,1.6vw,19px)] font-semibold">{item.q}</span>
                <svg
                  width="18"
                  height="18"
                  viewBox="0 0 24 24"
                  className="shrink-0 text-mute transition-transform duration-200 group-open:rotate-180"
                  aria-hidden="true"
                >
                  <path d="M6 9l6 6 6-6" stroke="currentColor" strokeWidth="2.4" strokeLinecap="round" strokeLinejoin="round" fill="none" />
                </svg>
              </summary>
              <p className="px-5 pb-6 text-[16px] leading-relaxed text-ink-dim sm:pl-[84px] sm:pr-8">{item.a}</p>
            </details>
          ))}
        </div>
      </div>
    </section>
  );
}
