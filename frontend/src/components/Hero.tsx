import { DownloadButton, DownloadMeta } from "./Bits";
import { ClackKeys } from "./ClackKeys";

export function Hero() {
  return (
    <section id="top" className="relative overflow-hidden">
      {/* A faint key-grid, like the plate under the keycaps. */}
      <div
        aria-hidden="true"
        className="pointer-events-none absolute inset-0 [background-image:linear-gradient(var(--color-grid)_1px,transparent_1px),linear-gradient(90deg,var(--color-grid)_1px,transparent_1px)] [background-size:64px_64px]"
      />
      <div className="relative mx-auto flex min-h-[calc(100svh-76px)] max-w-[1280px] flex-col items-center justify-center px-5 pt-20 pb-24 text-center sm:px-8">
        <ClackKeys />
        <p className="mt-10 font-mono text-[13px] text-mute">↑ go on, press them</p>

        <h1 className="mt-14 max-w-[1000px] font-display text-[clamp(34px,5.6vw,76px)] leading-[1.04] font-bold tracking-[-0.03em]">
          The keyboard sound your MacBook <span className="text-signal">forgot.</span>
        </h1>
        <p className="mt-7 max-w-[640px] text-[clamp(17px,1.5vw,20px)] leading-relaxed text-ink-dim">
          Keyfeel plays real recordings of mechanical switches the moment you press a key. 25
          packs, a tiny menu bar app, and nothing running while you&rsquo;re not typing.
        </p>
        <div className="mt-11 flex flex-col items-center gap-4">
          <DownloadButton />
          <DownloadMeta />
        </div>
      </div>
    </section>
  );
}
