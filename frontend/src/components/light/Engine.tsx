import { engine } from "@/lib/content";

import { DownloadButton } from "./Bits";
import { Card, IdleGraph, PanelPair, PanStrip, Row } from "./EngineParts";

export function Engine() {
  const { speed, privacy, idle, panning, panel, calls } = engine;
  return (
    <section id="l-engine" className="bg-[#0b0b0b] py-28 text-paper sm:py-32">
      <div className="mx-auto max-w-[1320px] px-5 sm:px-8">
        <div className="mx-auto max-w-[900px] text-center">
          <h2 className="font-display text-[clamp(44px,6.6vw,92px)] leading-[1] font-extrabold tracking-[-0.035em]">
            {engine.title.before}
            <span className="text-sun">{engine.title.mark}</span>
          </h2>
          <p className="mx-auto mt-6 max-w-[640px] text-[clamp(17px,1.5vw,20px)] leading-relaxed text-paper/70">
            {engine.body}
          </p>
          <div className="mt-10 flex justify-center">
            <DownloadButton tone="sun" />
          </div>
        </div>

        <div className="mt-16 grid gap-5 lg:grid-cols-12">
          <Card n={speed.n} title={speed.title} color="var(--color-sun-soft)" className="lg:col-span-7" body={speed.body}>
            <div className="w-full">
              <p className="font-display text-[clamp(72px,9vw,124px)] leading-none font-extrabold tracking-[-0.05em]">
                {speed.value}
                <span className="text-[0.4em] tracking-normal"> {speed.unit}</span>
              </p>
              <div className="mt-5 flex items-center gap-3">
                <span className="keycap grid h-11 w-11 shrink-0 place-items-center bg-white font-display font-bold [--depth:3px]">K</span>
                <span className="relative h-3 flex-1 rounded-full border-2 border-ink bg-white">
                  <span className="absolute inset-y-0 left-0 w-[14%] rounded-full bg-ink" />
                </span>
                <span className="grid h-11 w-11 shrink-0 place-items-center rounded-full border-2 border-ink bg-sun">
                  <svg width="20" height="20" viewBox="0 0 24 24" aria-hidden="true">
                    <path d="M4 9h4l5-4v14l-5-4H4z" fill="var(--color-ink)" />
                    <path d="M16 8.5a5 5 0 0 1 0 7" stroke="var(--color-ink)" strokeWidth="2.4" strokeLinecap="round" fill="none" />
                  </svg>
                </span>
              </div>
              <p className="mt-3 font-mono text-[12px] opacity-60">{speed.note}</p>
            </div>
          </Card>

          <Card n={privacy.n} title={privacy.title} color="var(--color-tang)" className="lg:col-span-5" body={privacy.body}>
            <ul className="w-full rounded-[20px] border-2 border-ink bg-white px-5 py-2">
              <Row yes>{privacy.used}</Row>
              {privacy.never.map((item) => (
                <Row key={item}>{item}</Row>
              ))}
            </ul>
          </Card>

          <Card n={idle.n} title={idle.title} color="var(--color-lilac)" className="lg:col-span-6 xl:col-span-3" body={idle.body}>
            <IdleGraph />
          </Card>

          <Card
            n={panning.n}
            title={panning.title}
            color="var(--color-olive)"
            className="lg:col-span-6 xl:col-span-3"
            body={panning.body}
          >
            <PanStrip />
          </Card>

          <Card n={panel.n} title={panel.title} color="var(--color-paper)" className="lg:col-span-6 xl:col-span-3" body={panel.body}>
            <PanelPair />
          </Card>

          <Card n={calls.n} title={calls.title} color="var(--color-sun)" className="lg:col-span-6 xl:col-span-3" body={calls.body}>
            <div className="flex w-full items-center gap-4 rounded-[20px] border-2 border-ink bg-white p-4">
              <span className="grid h-14 w-14 shrink-0 place-items-center rounded-full bg-ink">
                <svg width="26" height="26" viewBox="0 0 24 24" aria-hidden="true">
                  <rect x="9" y="3" width="6" height="11" rx="3" fill="var(--color-paper)" />
                  <path d="M6 11a6 6 0 0 0 12 0M12 17v3" stroke="var(--color-paper)" strokeWidth="2" strokeLinecap="round" fill="none" />
                  <path d="M4 4l16 16" stroke="var(--color-sun)" strokeWidth="2.4" strokeLinecap="round" />
                </svg>
              </span>
              <span className="text-[16px] leading-snug font-semibold">
                {calls.status}
                <span className="block text-[14px] font-normal text-ink-soft">{calls.note}</span>
              </span>
            </div>
          </Card>
        </div>
      </div>
    </section>
  );
}
