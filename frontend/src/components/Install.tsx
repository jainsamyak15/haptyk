import Link from "next/link";

import { install, nav } from "@/lib/content";
import { site } from "@/lib/site";

import { ArrowRight, Logo, Overline } from "./Bits";
import { MacMock } from "./MacMocks";
import { ThemeToggle } from "./ThemeToggle";

/** /download in the dark design: the download has started; here is how to open it. */
export function Install() {
  const steps = site.notarized ? install.verified : install.unverified;
  return (
    <>
      <header className="sticky top-0 z-50 rule-b bg-page/85 backdrop-blur-md">
        <div className="mx-auto flex h-[76px] max-w-[1280px] items-center justify-between gap-6 px-5 sm:px-8">
          <Link href="/" aria-label={nav.home}>
            <Logo />
          </Link>
          <ThemeToggle className="text-ink-dim edge hover:bg-raised hover:text-ink" />
        </div>
      </header>

      <main>
        <section className="relative overflow-hidden">
          <div
            aria-hidden="true"
            className="pointer-events-none absolute inset-0 [background-image:linear-gradient(var(--color-grid)_1px,transparent_1px),linear-gradient(90deg,var(--color-grid)_1px,transparent_1px)] [background-size:64px_64px]"
          />
          <div className="relative mx-auto max-w-[1280px] px-5 pt-20 pb-14 sm:px-8 sm:pt-24">
            <Overline>{install.eyebrow}</Overline>
            <h1 className="mt-5 font-display text-[clamp(34px,5.4vw,72px)] leading-[1.04] font-bold tracking-[-0.03em]">
              {install.title.before}
              <span className="text-signal">{install.title.mark}</span>
            </h1>
            <p className="mt-6 max-w-[620px] text-[clamp(17px,1.5vw,20px)] leading-relaxed text-ink-dim">{install.body}</p>
            <p className="mt-5 text-[15px] text-mute">
              {install.retry}{" "}
              <a href={site.dmgUrl} download className="font-semibold text-ink underline decoration-signal underline-offset-4">
                {install.retryLink}
              </a>
            </p>
          </div>
        </section>

        <section className="mx-auto max-w-[1280px] px-5 pb-24 sm:px-8">
          <ol className="grid gap-5 md:grid-cols-2">
            {steps.map((s, i) => (
              <li key={s.title} className="flex flex-col overflow-hidden rounded-[28px] bg-surface edge">
                <div className="p-7 sm:p-8">
                  <span className="font-mono text-[13px] font-medium text-signal">{String(i + 1).padStart(2, "0")}</span>
                  <h2 className="mt-2 font-display text-[clamp(22px,2vw,28px)] leading-tight font-bold tracking-tight">{s.title}</h2>
                  <p className="mt-3 max-w-[520px] text-[16px] leading-relaxed text-ink-dim">{s.body}</p>
                </div>
                <div className="mt-auto grid min-h-[250px] place-items-center rule-t bg-well px-4 py-8">
                  <MacMock kind={s.mock} />
                </div>
              </li>
            ))}
          </ol>
          {!site.notarized && <p className="mt-8 max-w-[760px] text-[15px] leading-relaxed text-mute">{install.why}</p>}
          <Link href="/" className="group mt-10 inline-flex items-center gap-2 text-[16px] font-bold text-signal">
            <span className="rotate-180 transition-transform duration-150 group-hover:-translate-x-1">
              <ArrowRight />
            </span>
            {install.back}
          </Link>
        </section>
      </main>
    </>
  );
}
