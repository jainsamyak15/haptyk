import Link from "next/link";

import { install, nav } from "@/lib/content";
import { site } from "@/lib/site";

import { MacMock } from "../MacMocks";
import { ThemeToggle } from "../ThemeToggle";
import { ArrowRight, Eyebrow, Logo, Mark } from "./Bits";

/** The colour behind each step's macOS drawing, in order. */
const TOPS = ["var(--color-sun-soft)", "var(--color-lilac)", "var(--color-olive)", "var(--color-tang)"];

/** /download in the light design: the download has started; here is how to open it. */
export function LightInstall() {
  const steps = site.notarized ? install.verified : install.unverified;
  return (
    <>
      <header className="sticky top-0 z-50 border-b-2 border-ink bg-paper/90 backdrop-blur-md">
        <div className="mx-auto flex h-[84px] max-w-[1320px] items-center justify-between gap-6 px-5 sm:px-8">
          <Link href="/" aria-label={nav.home}>
            <Logo />
          </Link>
          <ThemeToggle className="border-2 border-ink bg-white hover:bg-sun" />
        </div>
      </header>

      <main>
        <section className="mx-auto max-w-[900px] px-5 pt-20 pb-14 text-center sm:px-8 sm:pt-24">
          <Eyebrow color="var(--color-lilac)">{install.eyebrow}</Eyebrow>
          <h1 className="mt-6 font-display text-[clamp(42px,6.2vw,84px)] leading-[1] font-extrabold tracking-[-0.035em]">
            {install.title.before}
            <Mark color="var(--color-sun)">{install.title.mark}</Mark>
          </h1>
          <p className="mx-auto mt-6 max-w-[640px] text-[clamp(17px,1.5vw,20px)] leading-relaxed text-ink-soft">{install.body}</p>
          <p className="mt-5 text-[15px] text-ink-mute">
            {install.retry}{" "}
            <a href={site.dmgUrl} download className="font-bold text-ink underline decoration-2 underline-offset-4 hover:decoration-signal">
              {install.retryLink}
            </a>
          </p>
        </section>

        <section className="mx-auto max-w-[1320px] px-5 pb-24 sm:px-8">
          <ol className="grid gap-5 md:grid-cols-2">
            {steps.map((s, i) => (
              <li key={s.title} className="flex flex-col overflow-hidden rounded-[28px] border-2 border-ink bg-white">
                <div className="p-7 sm:p-8">
                  <span className="font-mono text-[13px] font-semibold opacity-70">{String(i + 1).padStart(2, "0")}</span>
                  <h2 className="mt-2 font-display text-[clamp(26px,2.4vw,34px)] leading-tight font-extrabold tracking-tight">{s.title}</h2>
                  <p className="mt-3 max-w-[540px] text-[17px] leading-relaxed text-ink-soft">{s.body}</p>
                </div>
                <div
                  className="mt-auto grid min-h-[250px] place-items-center border-t-2 border-ink px-4 py-8"
                  style={{ background: TOPS[i % TOPS.length] }}
                >
                  <MacMock kind={s.mock} />
                </div>
              </li>
            ))}
          </ol>
          {!site.notarized && <p className="mt-8 max-w-[760px] text-[16px] leading-relaxed text-ink-soft">{install.why}</p>}
          <Link href="/" className="group mt-10 inline-flex items-center gap-2 text-[17px] font-bold">
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
