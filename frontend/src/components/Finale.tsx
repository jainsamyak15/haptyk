import { Cap, DownloadButton, DownloadMeta, Logo } from "./Bits";

export function FinalCta() {
  return (
    <section className="px-5 pb-24 sm:px-8">
      <div className="relative mx-auto max-w-[1280px] overflow-hidden rounded-[36px] bg-surface px-6 py-24 text-center edge sm:py-28">
        {/* A few loose keycaps scattered around the edges. */}
        <Cap tone="mod" className="absolute top-10 left-[6%] hidden h-20 w-20 rotate-[-14deg] md:block">
          <span className="font-mono text-[13px]">esc</span>
        </Cap>
        <Cap className="absolute bottom-12 left-[12%] hidden h-16 w-16 rotate-[9deg] lg:block">
          <span className="font-display text-[22px] font-bold">K</span>
        </Cap>
        <Cap tone="signal" className="absolute top-14 right-[8%] hidden h-[72px] w-[72px] rotate-[12deg] md:block">
          <span className="font-mono text-[26px] leading-none">⏎</span>
        </Cap>
        <Cap className="absolute right-[14%] bottom-10 hidden h-14 w-[120px] rotate-[-6deg] lg:block" />

        <h2 className="relative mx-auto max-w-[820px] font-display text-[clamp(34px,5.4vw,72px)] leading-[1.04] font-bold tracking-[-0.03em]">
          Go make some <span className="text-signal">noise.</span>
        </h2>
        <p className="relative mx-auto mt-6 max-w-[520px] text-[18px] leading-relaxed text-ink-dim">
          Download Keyfeel, pick a switch, and hear yourself type.
        </p>
        <div className="relative mt-11 flex flex-col items-center gap-4">
          <DownloadButton />
          <DownloadMeta />
        </div>
      </div>
    </section>
  );
}

export function Footer() {
  return (
    <footer className="rule-t">
      <div className="mx-auto flex max-w-[1280px] flex-col gap-8 px-5 py-12 sm:px-8 md:flex-row md:items-start md:justify-between">
        <div>
          <Logo size={32} />
          <p className="mt-3 text-[14px] text-mute">© 2026 The Keyfeel Authors.</p>
        </div>
        <p className="max-w-[560px] text-[14px] leading-relaxed text-mute">
          Demo sounds on this page: NK Cream, Cherry MX Blue and Topre from{" "}
          <a className="underline underline-offset-2 hover:text-ink" href="https://github.com/hainguyents13/mechvibes">
            Mechvibes
          </a>{" "}
          (MIT, © 2021 Hai Nguyen,{" "}
          <a className="underline underline-offset-2 hover:text-ink" href="/sounds/LICENSE-mechvibes.txt">
            licence
          </a>
          ), and Holy Panda from{" "}
          <a className="underline underline-offset-2 hover:text-ink" href="https://github.com/tplai/kbsim">
            kbsim
          </a>{" "}
          (MIT, © Thomas Lai,{" "}
          <a className="underline underline-offset-2 hover:text-ink" href="/sounds/LICENSE-kbsim.txt">
            licence
          </a>
          ).
        </p>
        <nav aria-label="Footer" className="flex gap-6 text-[14px] font-semibold text-ink-dim">
          <a href="#switches" className="hover:text-ink">
            Switches
          </a>
          <a href="#try" className="hover:text-ink">
            Try it
          </a>
          <a href="#faq" className="hover:text-ink">
            FAQ
          </a>
        </nav>
      </div>
    </footer>
  );
}
