"use client";

import { tryIt } from "@/lib/content";
import { selectDemoPack } from "@/lib/demo-store";
import { demoPacks } from "@/lib/packs";
import { ROWS, TAP_PAD, useTypingDemo } from "@/lib/typing-demo";

import { Eyebrow, Mark, Stem } from "./Bits";
import { kindColor } from "./palette";

const accent: Record<string, string> = {
  Backspace: "var(--color-tang)",
  Enter: "var(--color-sun)",
};

export function LightTypeDemo() {
  const {
    packId,
    pack,
    setFocused,
    muted,
    down,
    setDown,
    text,
    listening,
    loading,
    pad,
    start,
    press,
    release,
    onKeyDown,
    toggleMute,
  } = useTypingDemo();

  return (
    <section id="l-try" className="relative py-28 sm:py-32">
      <div className="mx-auto max-w-[1320px] px-5 sm:px-8">
        <div className="mx-auto max-w-[820px] text-center">
          <Eyebrow color="var(--color-lilac)">{tryIt.eyebrow}</Eyebrow>
          <h2 className="mt-6 font-display text-[clamp(42px,6.2vw,84px)] leading-[1] font-extrabold tracking-[-0.035em]">
            {tryIt.title.before}
            <Mark color="var(--color-sun)">{tryIt.title.mark}</Mark>
          </h2>
          <p className="mt-6 text-[clamp(17px,1.5vw,20px)] leading-relaxed text-ink-soft">{tryIt.body}</p>
        </div>

        {/* minmax(0, …): the one-line text box must not set the column's width. */}
        <div className="mt-14 grid grid-cols-1 items-start gap-6 lg:grid-cols-[290px_minmax(0,1fr)]">
          <div
            role="radiogroup"
            aria-label={tryIt.picker}
            className="flex gap-3 overflow-x-auto pb-2 lg:flex-col lg:overflow-visible lg:pb-0"
          >
            {demoPacks.map((p) => {
              const on = p.id === packId;
              return (
                <button
                  key={p.id}
                  type="button"
                  role="radio"
                  aria-checked={on}
                  onClick={() => {
                    start();
                    selectDemoPack(p.id);
                    pad.current?.focus({ preventScroll: true });
                  }}
                  className={`flex min-w-[230px] items-center gap-3.5 rounded-[20px] border-2 border-ink p-3.5 text-left transition-[transform,background-color] duration-150 hover:-translate-y-0.5 lg:min-w-0 ${
                    on ? "bg-ink text-paper" : "bg-white"
                  }`}
                >
                  <Stem kind={p.kind} size={46} />
                  <span>
                    <span className="block font-display text-[20px] leading-tight font-bold">{p.name}</span>
                    <span className={`block text-[14px] ${on ? "text-paper/70" : "text-ink-mute"}`}>{p.blurb}</span>
                  </span>
                </button>
              );
            })}
          </div>

          <div
            id="l-try-pad"
            ref={pad}
            tabIndex={0}
            role="application"
            aria-label={tryIt.pad(pack.name)}
            onFocus={() => {
              setFocused(true);
              start();
            }}
            onBlur={(e) => {
              // React's blur bubbles: only stop listening when focus leaves the demo entirely.
              if (!e.currentTarget.contains(e.relatedTarget as Node | null)) {
                setFocused(false);
                setDown(new Set());
              }
            }}
            onKeyDown={onKeyDown}
            onKeyUp={(e) => release(e.code)}
            className="relative min-w-0 rounded-[32px] border-2 border-ink bg-stone p-4 sm:p-6"
          >
            <div className="mb-5 flex items-center gap-3">
              <div className="flex min-h-[64px] min-w-0 flex-1 items-center overflow-hidden rounded-[18px] border-2 border-ink bg-white px-5">
                <p className="truncate font-display text-[clamp(20px,2.4vw,30px)] font-semibold tracking-tight [direction:rtl]">
                  <bdi>
                    {text ? (
                      text
                    ) : (
                      <span className="text-ink-mute">{listening ? tryIt.listening : tryIt.empty}</span>
                    )}
                    {listening && <span className="ml-0.5 inline-block w-[3px] animate-pulse bg-ink">&nbsp;</span>}
                  </bdi>
                </p>
              </div>
              <button
                type="button"
                onPointerDown={(e) => e.preventDefault()} // Keep focus, and typing, on the demo.
                onClick={toggleMute}
                aria-pressed={muted}
                aria-label={muted ? tryIt.unmute : tryIt.mute}
                className="grid h-[64px] w-[64px] shrink-0 place-items-center rounded-[18px] border-2 border-ink bg-white transition-colors hover:bg-sun"
              >
                <svg width="26" height="26" viewBox="0 0 24 24" aria-hidden="true">
                  <path d="M4 9h4l5-4v14l-5-4H4z" fill="var(--color-ink)" />
                  {muted ? (
                    <path d="M16 9l5 6M21 9l-5 6" stroke="var(--color-ink)" strokeWidth="2.4" strokeLinecap="round" />
                  ) : (
                    <path
                      d="M16 8.5a5 5 0 0 1 0 7M18.6 6a8.5 8.5 0 0 1 0 12"
                      stroke="var(--color-ink)"
                      strokeWidth="2.4"
                      strokeLinecap="round"
                      fill="none"
                    />
                  )}
                </svg>
              </button>
            </div>

            <div className="kb relative [container-type:inline-size]">
              <div className="flex flex-col gap-[max(4px,0.9cqi)]">
                {ROWS.map((row, i) => (
                  <div
                    key={i}
                    className="grid gap-[max(4px,0.9cqi)]"
                    style={{ gridTemplateColumns: row.map((k) => `${k.w ?? 1}fr`).join(" ") }}
                  >
                    {row.map((k) => {
                      const isDown = down.has(k.code);
                      const bg = k.code === "Space" ? kindColor[pack.kind] : (accent[k.code] ?? "#fff");
                      return (
                        <button
                          key={k.code}
                          type="button"
                          tabIndex={-1}
                          aria-hidden="true"
                          data-down={isDown}
                          onPointerDown={(e) => {
                            e.preventDefault();
                            pad.current?.focus({ preventScroll: true });
                            start();
                            press(k.code, k.label.length === 1 ? k.label.toLowerCase() : k.code === "Space" ? " " : undefined);
                          }}
                          onPointerUp={() => release(k.code)}
                          onPointerLeave={() => release(k.code)}
                          className={`keycap flex h-[max(30px,5.6cqi)] items-center justify-center rounded-[max(7px,1.1cqi)] leading-none [--depth:max(3px,0.5cqi)] ${
                            k.mod ? "font-mono text-[max(8px,1.35cqi)] font-semibold lowercase" : "font-display text-[max(11px,2.1cqi)] font-bold"
                          }`}
                          style={{ background: isDown ? (bg === "var(--color-sun)" ? "#fff" : "var(--color-sun)") : bg }}
                        >
                          {k.label}
                        </button>
                      );
                    })}
                  </div>
                ))}
              </div>

              {!listening && (
                <button
                  type="button"
                  tabIndex={-1} // The demo itself is the tab stop; this only catches the pointer.
                  onPointerDown={(e) => {
                    // Never take focus: this button vanishes once the demo listens, and a
                    // focused element that vanishes drops focus to the page.
                    e.preventDefault();
                    start();
                    pad.current?.focus({ preventScroll: true });
                  }}
                  className="absolute inset-[-6px] grid place-items-center rounded-[18px] bg-stone/70 backdrop-blur-[2px]"
                >
                  <span className="inline-flex items-center gap-3 rounded-full bg-ink px-7 py-4 text-[18px] font-bold text-paper shadow-[0_6px_0_rgba(11,11,11,0.25)]">
                    <span className="relative flex h-3 w-3">
                      <span className="absolute inline-flex h-full w-full animate-ping rounded-full bg-sun opacity-75" />
                      <span className="relative inline-flex h-3 w-3 rounded-full bg-sun" />
                    </span>
                    {tryIt.start}
                  </span>
                </button>
              )}
            </div>

            <div className="coarse-only mt-5 grid-cols-4 gap-3">
              {TAP_PAD.map((t) => (
                <button
                  key={t.code}
                  type="button"
                  data-down={down.has(`tap-${t.code}`)}
                  onPointerDown={(e) => {
                    e.preventDefault();
                    start();
                    press(t.code, t.code === "KeyK" ? "k" : t.code === "Space" ? " " : undefined);
                    setDown((d) => new Set(d).add(`tap-${t.code}`));
                  }}
                  onPointerUp={() => {
                    release(t.code);
                    release(`tap-${t.code}`);
                  }}
                  onPointerLeave={() => release(`tap-${t.code}`)}
                  className="keycap h-16 bg-white text-[16px] font-bold"
                >
                  {t.label}
                </button>
              ))}
            </div>

            <p className="mt-5 flex flex-wrap items-center justify-between gap-2 text-[14px] text-ink-soft" aria-live="polite">
              <span>
                {loading ? tryIt.loading(pack.name) : listening ? tryIt.playing(pack.name) : tryIt.ready(pack.name)}
              </span>
              <span className="text-ink-mute">{tryIt.note}</span>
            </p>
          </div>
        </div>
      </div>
    </section>
  );
}
