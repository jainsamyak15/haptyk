"use client";

import { tryIt } from "@/lib/content";
import { selectDemoPack } from "@/lib/demo-store";
import { demoPacks } from "@/lib/packs";
import { ROWS, TAP_PAD, useTypingDemo } from "@/lib/typing-demo";

import { Overline, Stem } from "./Bits";

/* Return and Delete wear the orange accent, as an Escape key would on a custom set. */
const SIGNAL = new Set(["Enter", "Backspace"]);

export function TypeDemo() {
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
    <section id="try" className="relative py-28 sm:py-32">
      <div className="mx-auto max-w-[1280px] px-5 sm:px-8">
        <Overline>{tryIt.eyebrow}</Overline>
        <h2 className="mt-5 max-w-[820px] font-display text-[clamp(32px,4.6vw,60px)] leading-[1.05] font-bold tracking-[-0.03em]">
          {tryIt.title.before}
          <span className="text-signal">{tryIt.title.mark}</span>
        </h2>
        <p className="mt-5 max-w-[620px] text-[18px] leading-relaxed text-ink-dim">{tryIt.body}</p>

        {/* minmax(0, …): the one-line text box must not set the column's width. */}
        <div className="mt-14 grid grid-cols-1 items-start gap-6 lg:grid-cols-[280px_minmax(0,1fr)]">
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
                  className={`flex min-w-[220px] items-center gap-3.5 rounded-[20px] p-3.5 text-left transition-colors lg:min-w-0 ${
                    on ? "bg-raised ring-2 ring-signal" : "bg-surface edge hover:bg-raised"
                  }`}
                >
                  <Stem kind={p.kind} size={44} />
                  <span>
                    <span className="block font-display text-[16px] leading-tight font-bold">{p.name}</span>
                    <span className="mt-0.5 block text-[13px] text-mute">
                      {p.blurb}
                    </span>
                  </span>
                </button>
              );
            })}
          </div>

          <div
            id="try-pad"
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
            className="relative min-w-0 rounded-[30px] bg-surface p-4 edge sm:p-6"
          >
            <div className="mb-6 flex items-center gap-3">
              <div className="flex min-h-[62px] min-w-0 flex-1 items-center overflow-hidden rounded-[16px] bg-well px-5 edge">
                <p className="truncate font-mono text-[clamp(17px,2vw,24px)] font-medium [direction:rtl]">
                  <bdi>
                    {text ? (
                      text
                    ) : (
                      <span className="text-mute">{listening ? tryIt.listening : tryIt.empty}</span>
                    )}
                    {listening && <span className="ml-0.5 inline-block w-[2px] animate-blink-caret bg-signal">&nbsp;</span>}
                  </bdi>
                </p>
              </div>
              <button
                type="button"
                onPointerDown={(e) => e.preventDefault()} // Keep focus, and typing, on the demo.
                onClick={toggleMute}
                aria-pressed={muted}
                aria-label={muted ? tryIt.unmute : tryIt.mute}
                className="grid h-[62px] w-[62px] shrink-0 place-items-center rounded-[16px] bg-well edge transition-colors hover:bg-raised"
              >
                <svg width="24" height="24" viewBox="0 0 24 24" aria-hidden="true">
                  <path d="M4 9h4l5-4v14l-5-4H4z" fill="var(--color-ink)" />
                  {muted ? (
                    <path d="M16 9l5 6M21 9l-5 6" stroke="var(--color-signal)" strokeWidth="2.4" strokeLinecap="round" />
                  ) : (
                    <path
                      d="M16 8.5a5 5 0 0 1 0 7M18.6 6a8.5 8.5 0 0 1 0 12"
                      stroke="var(--color-ink)"
                      strokeWidth="2.2"
                      strokeLinecap="round"
                      fill="none"
                    />
                  )}
                </svg>
              </button>
            </div>

            <div className="kb relative [container-type:inline-size]">
              <div className="flex flex-col gap-[max(4px,0.8cqi)]">
                {ROWS.map((row, i) => (
                  <div
                    key={i}
                    className="grid gap-[max(4px,0.8cqi)]"
                    style={{ gridTemplateColumns: row.map((k) => `${k.w ?? 1}fr`).join(" ") }}
                  >
                    {row.map((k) => (
                      <button
                        key={k.code}
                        type="button"
                        tabIndex={-1}
                        aria-hidden="true"
                        data-down={down.has(k.code)}
                        onPointerDown={(e) => {
                          e.preventDefault();
                          pad.current?.focus({ preventScroll: true });
                          start();
                          press(k.code, k.label.length === 1 ? k.label.toLowerCase() : k.code === "Space" ? " " : undefined);
                        }}
                        onPointerUp={() => release(k.code)}
                        onPointerLeave={() => release(k.code)}
                        className={`cap ${SIGNAL.has(k.code) ? "cap-signal" : k.mod ? "cap-mod" : ""} h-[max(34px,5.8cqi)] [--depth:max(4px,0.65cqi)] [--r:max(7px,1.1cqi)]`}
                      >
                        <span
                          className={`cap-top leading-none ${
                            k.mod
                              ? "font-mono text-[max(8px,1.3cqi)] font-medium lowercase"
                              : "font-display text-[max(10px,1.8cqi)] font-bold"
                          }`}
                        >
                          {k.label}
                        </span>
                      </button>
                    ))}
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
                  className="absolute inset-[-8px] grid place-items-center rounded-[18px] bg-surface/75 backdrop-blur-[3px]"
                >
                  <span className="inline-flex items-center gap-3 rounded-full bg-signal px-7 py-4 text-[17px] font-bold text-signal-ink">
                    <span className="relative flex h-2.5 w-2.5">
                      <span className="absolute inline-flex h-full w-full animate-ping rounded-full bg-signal-ink opacity-50" />
                      <span className="relative inline-flex h-2.5 w-2.5 rounded-full bg-signal-ink" />
                    </span>
                    {tryIt.start}
                  </span>
                </button>
              )}
            </div>

            <div className="coarse-only mt-6 grid-cols-4 gap-3">
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
                  className={`cap ${t.code === "Enter" || t.code === "Backspace" ? "cap-signal" : ""} h-16 [--depth:6px] [--r:14px]`}
                >
                  <span className="cap-top text-[15px] font-bold">{t.label}</span>
                </button>
              ))}
            </div>

            <p
              className="mt-5 flex flex-wrap items-center justify-between gap-2 font-mono text-[12px] text-mute"
              aria-live="polite"
            >
              <span className="text-ink-dim">
                {loading ? tryIt.loading(pack.name) : listening ? tryIt.playing(pack.name) : tryIt.ready(pack.name)}
              </span>
              <span>{tryIt.note}</span>
            </p>
          </div>
        </div>
      </div>
    </section>
  );
}
