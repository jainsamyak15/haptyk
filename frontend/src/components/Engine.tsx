import { engine } from "@/lib/content";

import { Cap, DownloadButton } from "./Bits";
import { Card, IdleGraph, PanelPair, PanStrip, Row } from "./EngineParts";

export function Engine() {
  const { speed, privacy, idle, panning, panel, calls } = engine;
  return (
    <section id="engine" className="rule-t py-28 sm:py-32">
      <div className="mx-auto max-w-[1280px] px-5 sm:px-8">
        <div className="flex flex-col gap-8 lg:flex-row lg:items-end lg:justify-between">
          <div>
            <h2 className="max-w-[900px] font-display text-[clamp(32px,4.6vw,60px)] leading-[1.05] font-bold tracking-[-0.03em]">
              {engine.title.before}
              <span className="text-signal">{engine.title.mark}</span>
            </h2>
            <p className="mt-5 max-w-[620px] text-[18px] leading-relaxed text-ink-dim">{engine.body}</p>
          </div>
          <div className="shrink-0">
            <DownloadButton />
          </div>
        </div>

        <div className="mt-14 grid gap-5 lg:grid-cols-12">
          <Card n={speed.n} title={speed.title} className="lg:col-span-7" body={speed.body}>
            <div className="w-full">
              <p className="font-display text-[clamp(72px,9vw,124px)] leading-none font-bold tracking-[-0.05em]">
                {speed.value}
                <span className="text-[0.4em] tracking-normal text-signal"> {speed.unit}</span>
              </p>
              <div className="mt-5 flex items-center gap-3">
                <Cap className="h-11 w-11 shrink-0 [--depth:5px] [--r:11px]">
                  <span className="font-display text-[15px] font-bold">K</span>
                </Cap>
                <span className="relative h-3 flex-1 rounded-full bg-well edge">
                  <span className="absolute inset-y-0 left-0 w-[14%] rounded-full bg-signal" />
                </span>
                <span className="grid h-11 w-11 shrink-0 place-items-center rounded-full bg-signal">
                  <svg width="20" height="20" viewBox="0 0 24 24" aria-hidden="true">
                    <path d="M4 9h4l5-4v14l-5-4H4z" fill="var(--color-signal-ink)" />
                    <path d="M16 8.5a5 5 0 0 1 0 7" stroke="var(--color-signal-ink)" strokeWidth="2.4" strokeLinecap="round" fill="none" />
                  </svg>
                </span>
              </div>
              <p className="mt-3 font-mono text-[12px] text-mute">{speed.note}</p>
            </div>
          </Card>

          <Card n={privacy.n} title={privacy.title} className="lg:col-span-5" body={privacy.body}>
            <ul className="w-full rounded-[20px] bg-well px-5 py-2 edge">
              <Row yes>{privacy.used}</Row>
              {privacy.never.map((item) => (
                <Row key={item}>{item}</Row>
              ))}
            </ul>
          </Card>

          <Card n={idle.n} title={idle.title} className="lg:col-span-6 xl:col-span-3" body={idle.body}>
            <IdleGraph />
          </Card>

          <Card n={panning.n} title={panning.title} className="lg:col-span-6 xl:col-span-3" body={panning.body}>
            <PanStrip />
          </Card>

          <Card n={panel.n} title={panel.title} className="lg:col-span-6 xl:col-span-3" body={panel.body}>
            <PanelPair />
          </Card>

          <Card n={calls.n} title={calls.title} className="lg:col-span-6 xl:col-span-3" body={calls.body}>
            <div className="flex w-full items-center gap-4 rounded-[20px] bg-well p-4 edge">
              <span className="grid h-14 w-14 shrink-0 place-items-center rounded-full bg-raised edge">
                <svg width="26" height="26" viewBox="0 0 24 24" aria-hidden="true">
                  <rect x="9" y="3" width="6" height="11" rx="3" fill="var(--color-ink)" />
                  <path d="M6 11a6 6 0 0 0 12 0M12 17v3" stroke="var(--color-ink)" strokeWidth="2" strokeLinecap="round" fill="none" />
                  <path d="M4 4l16 16" stroke="var(--color-signal)" strokeWidth="2.4" strokeLinecap="round" />
                </svg>
              </span>
              <span className="text-[16px] leading-snug font-semibold">
                {calls.status}
                <span className="block text-[14px] font-normal text-mute">{calls.note}</span>
              </span>
            </div>
          </Card>
        </div>
      </div>
    </section>
  );
}
