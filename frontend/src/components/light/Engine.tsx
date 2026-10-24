import { DownloadButton } from "./Bits";
import { Card, IdleGraph, PanelPair, PanStrip, Row } from "./EngineParts";

/* Numbers here are measured (docs/PERFORMANCE.md) or are the app's defaults. */
export function Engine() {
  return (
    <section id="l-engine" className="bg-[#0b0b0b] py-28 text-paper sm:py-32">
      <div className="mx-auto max-w-[1320px] px-5 sm:px-8">
        <div className="mx-auto max-w-[900px] text-center">
          <h2 className="font-display text-[clamp(44px,6.6vw,92px)] leading-[1] font-extrabold tracking-[-0.035em]">
            The tiny engine behind every <span className="text-sun">clack.</span>
          </h2>
          <p className="mx-auto mt-6 max-w-[640px] text-[clamp(17px,1.5vw,20px)] leading-relaxed text-paper/70">
            Written in C from the audio thread up, and measured at every milestone, not just promised.
          </p>
          <div className="mt-10 flex justify-center">
            <DownloadButton tone="sun" />
          </div>
        </div>

        <div className="mt-16 grid gap-5 lg:grid-cols-12">
          <Card
            n="01"
            title="Faster than your finger"
            color="var(--color-sun-soft)"
            className="lg:col-span-7"
            body="Sound starts within a single 128-frame audio buffer. Getting from your key to a queued sound takes under a microsecond."
          >
            <div className="w-full">
              <p className="font-display text-[clamp(72px,9vw,124px)] leading-none font-extrabold tracking-[-0.05em]">
                2.7<span className="text-[0.4em] tracking-normal"> ms</span>
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
              <p className="mt-3 font-mono text-[12px] opacity-60">one buffer at 48 kHz · key to sound queued: under 1 µs (p99, measured)</p>
            </div>
          </Card>

          <Card
            n="02"
            title="Remembers nothing you type"
            color="var(--color-tang)"
            className="lg:col-span-5"
            body="A key press picks a sound, and that is all it is used for."
          >
            <ul className="w-full rounded-[20px] border-2 border-ink bg-white px-5 py-2">
              <Row yes>Which sound to play</Row>
              <Row>The text you type</Row>
              <Row>Key codes, once the sound is picked</Row>
              <Row>App names, window titles, URLs</Row>
            </ul>
          </Card>

          <Card
            n="03"
            title="Asleep until you type"
            color="var(--color-lilac)"
            className="lg:col-span-6 xl:col-span-3"
            body="No timers, no polling. Thirty seconds after your last key, audio stops."
          >
            <IdleGraph />
          </Card>

          <Card
            n="04"
            title="Keys sound where they are"
            color="var(--color-olive)"
            className="lg:col-span-6 xl:col-span-3"
            body="Each key is panned to its place on the keyboard, so Q comes from the left and P from the right."
          >
            <PanStrip />
          </Card>

          <Card
            n="05"
            title="Graphite or Glass"
            color="var(--color-paper)"
            className="lg:col-span-6 xl:col-span-3"
            body="A menu bar panel with your packs, volume and stats, in warm Graphite or Apple-style Glass."
          >
            <PanelPair />
          </Card>

          <Card
            n="06"
            title="Quiet on calls"
            color="var(--color-sun)"
            className="lg:col-span-6 xl:col-span-3"
            body="When an app starts using your microphone, Keyfeel goes quiet on its own. Nobody on the call hears you clacking."
          >
            <div className="flex w-full items-center gap-4 rounded-[20px] border-2 border-ink bg-white p-4">
              <span className="grid h-14 w-14 shrink-0 place-items-center rounded-full bg-ink">
                <svg width="26" height="26" viewBox="0 0 24 24" aria-hidden="true">
                  <rect x="9" y="3" width="6" height="11" rx="3" fill="var(--color-paper)" />
                  <path d="M6 11a6 6 0 0 0 12 0M12 17v3" stroke="var(--color-paper)" strokeWidth="2" strokeLinecap="round" fill="none" />
                  <path d="M4 4l16 16" stroke="var(--color-sun)" strokeWidth="2.4" strokeLinecap="round" />
                </svg>
              </span>
              <span className="text-[16px] leading-snug font-semibold">
                Mic in use, sounds paused
                <span className="block text-[14px] font-normal text-ink-soft">On by default</span>
              </span>
            </div>
          </Card>
        </div>
      </div>
    </section>
  );
}
