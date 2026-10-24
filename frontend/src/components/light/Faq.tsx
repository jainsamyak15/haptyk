import { faq } from "@/lib/content";
import { FAQ } from "@/lib/faq";

import { Mark } from "./Bits";

export function LightFaq() {
  return (
    <section id="l-faq" className="border-t-2 border-ink py-28 sm:py-32">
      <div className="mx-auto grid max-w-[1320px] gap-12 px-5 sm:px-8 lg:grid-cols-[minmax(0,5fr)_minmax(0,8fr)] lg:gap-20">
        <h2 className="font-display text-[clamp(46px,6vw,84px)] leading-[1] font-extrabold tracking-[-0.035em] lg:sticky lg:top-32 lg:self-start">
          {faq.title.before}
          <br />
          <Mark color="var(--color-lilac)">{faq.title.mark}</Mark>
        </h2>
        <div className="border-t-2 border-ink">
          {FAQ.map((item, i) => (
            <details key={item.q} className="faq group border-b-2 border-ink" open={i === 0}>
              <summary className="flex cursor-pointer items-center justify-between gap-6 py-7 text-[clamp(18px,1.7vw,22px)] font-medium">
                {item.q}
                <span
                  className="faq-icon grid h-11 w-11 shrink-0 place-items-center rounded-full border-2 border-ink transition-colors group-hover:bg-sun-soft"
                  aria-hidden="true"
                >
                  <svg width="16" height="16" viewBox="0 0 16 16">
                    <path d="M2 8h12" stroke="var(--color-ink)" strokeWidth="2.4" strokeLinecap="round" />
                    <path
                      className="faq-plus-v origin-center transition-transform duration-200"
                      d="M8 2v12"
                      stroke="var(--color-ink)"
                      strokeWidth="2.4"
                      strokeLinecap="round"
                    />
                  </svg>
                </span>
              </summary>
              <p className="max-w-[760px] pb-8 text-[17px] leading-relaxed text-ink-soft">{item.a}</p>
            </details>
          ))}
        </div>
      </div>
    </section>
  );
}
