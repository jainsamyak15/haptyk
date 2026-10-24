import { Buddy } from "./Buddy";
import { DownloadButton, DownloadMeta, Logo, Mark, Sparkle } from "./Bits";

export function LightFinalCta() {
  return (
    <section className="px-5 pb-24 sm:px-8">
      <div className="relative mx-auto max-w-[1320px] overflow-hidden rounded-[40px] border-2 border-ink bg-sun px-6 py-24 text-center sm:py-28">
        <Buddy
          shape="round"
          color="var(--color-tang)"
          size={190}
          rotate={-14}
          look={[-0.6, 0.4]}
          blinkDelay={0.6}
          className="-top-10 -right-8 origin-top-right scale-50 sm:scale-75 lg:scale-100"
        />
        <Buddy
          shape="keycap"
          color="var(--color-lilac)"
          size={170}
          rotate={20}
          look={[0.7, -0.4]}
          blinkDelay={2.6}
          floatDelay={1.2}
          className="-bottom-12 -left-6 origin-bottom-left scale-50 sm:scale-75 lg:scale-100"
        />
        <Sparkle color="var(--color-ink)" size={46} className="absolute top-[18%] left-[12%] hidden animate-twinkle md:block" />
        <Sparkle
          color="var(--color-volt)"
          size={38}
          className="absolute right-[16%] bottom-[18%] hidden animate-twinkle md:block"
          style={{ animationDelay: "1.4s" }}
        />
        <h2 className="relative mx-auto max-w-[900px] font-display text-[clamp(44px,6.4vw,92px)] leading-[1.02] font-extrabold tracking-[-0.035em]">
          Ready to hear yourself{" "}
          <Mark color="var(--color-ink)">
            <span className="text-paper">type?</span>
          </Mark>
        </h2>
        <div className="relative mt-11 flex flex-col items-center gap-4">
          <DownloadButton />
          <DownloadMeta className="text-ink/70" />
        </div>
      </div>
    </section>
  );
}

export function LightFooter() {
  return (
    <footer className="border-t-2 border-ink">
      <div className="mx-auto flex max-w-[1320px] flex-col gap-8 px-5 py-12 sm:px-8 md:flex-row md:items-start md:justify-between">
        <div>
          <Logo size={34} />
          <p className="mt-3 text-[14px] text-ink-mute">© 2026 The Keyfeel Authors.</p>
        </div>
        <p className="max-w-[560px] text-[14px] leading-relaxed text-ink-mute">
          Demo sounds on this page: NK Cream, Cherry MX Blue and Topre from{" "}
          <a className="underline underline-offset-2" href="https://github.com/hainguyents13/mechvibes">
            Mechvibes
          </a>{" "}
          (MIT, © 2021 Hai Nguyen,{" "}
          <a className="underline underline-offset-2" href="/sounds/LICENSE-mechvibes.txt">
            licence
          </a>
          ), and Holy Panda from{" "}
          <a className="underline underline-offset-2" href="https://github.com/tplai/kbsim">
            kbsim
          </a>{" "}
          (MIT, © Thomas Lai,{" "}
          <a className="underline underline-offset-2" href="/sounds/LICENSE-kbsim.txt">
            licence
          </a>
          ).
        </p>
        <nav aria-label="Footer" className="flex gap-6 text-[15px] font-semibold">
          <a href="#l-try" className="hover:underline">
            Try it
          </a>
          <a href="#l-switches" className="hover:underline">
            Switches
          </a>
          <a href="#l-faq" className="hover:underline">
            FAQ
          </a>
        </nav>
      </div>
    </footer>
  );
}
