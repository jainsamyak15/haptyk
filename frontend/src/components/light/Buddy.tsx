"use client";

import { type CSSProperties, useEffect, useRef } from "react";

import { PRESS_EVENT } from "@/lib/demo-store";

/* Every eye on the page follows the pointer through one listener, batched per frame.
 * `reach` is in the SVG's own units; `turn` is the character's tilt, undone so the gaze
 * points at the pointer on screen rather than in the rotated drawing. */
type Eye = { anchor: SVGCircleElement; pupil: SVGGElement; reach: number; turn: number };
const eyes = new Set<Eye>();
let px = 0;
let py = 0;
let frame = 0;

function aim() {
  frame = 0;
  for (const eye of eyes) {
    const box = eye.anchor.getBoundingClientRect();
    if (box.width === 0) {
      continue; // This design is hidden (the other appearance is showing).
    }
    const dx = px - (box.left + box.width / 2);
    const dy = py - (box.top + box.height / 2);
    const d = Math.hypot(dx, dy) || 1;
    const pull = Math.min(1, d / 260) * eye.reach;
    const sx = (dx / d) * pull;
    const sy = (dy / d) * pull;
    const t = (-eye.turn * Math.PI) / 180;
    const x = sx * Math.cos(t) - sy * Math.sin(t);
    const y = sx * Math.sin(t) + sy * Math.cos(t);
    eye.pupil.setAttribute("transform", `translate(${x.toFixed(1)} ${y.toFixed(1)})`);
  }
}

function onMove(e: PointerEvent) {
  px = e.clientX;
  py = e.clientY;
  if (frame === 0) {
    frame = requestAnimationFrame(aim);
  }
}

function track(eye: Eye) {
  if (eyes.size === 0) {
    window.addEventListener("pointermove", onMove, { passive: true });
  }
  eyes.add(eye);
  return () => {
    eyes.delete(eye);
    if (eyes.size === 0) {
      window.removeEventListener("pointermove", onMove);
    }
  };
}

function GooglyEye({
  cx,
  cy,
  r,
  look,
  turn,
  blinkDelay,
}: {
  cx: number;
  cy: number;
  r: number;
  look: [number, number];
  turn: number;
  blinkDelay: number;
}) {
  const anchor = useRef<SVGCircleElement>(null);
  const pupil = useRef<SVGGElement>(null);
  const reach = r * 0.42;

  useEffect(() => {
    const a = anchor.current;
    const p = pupil.current;
    if (!a || !p || window.matchMedia("(prefers-reduced-motion: reduce)").matches) {
      return;
    }
    return track({ anchor: a, pupil: p, reach, turn });
  }, [reach, turn]);

  const pr = r * 0.56;
  return (
    <g className="eyelid animate-blink" style={{ animationDelay: `${blinkDelay}s` }}>
      <circle ref={anchor} cx={cx} cy={cy} r={r} fill="#fff" />
      <g ref={pupil} transform={`translate(${look[0] * reach} ${look[1] * reach})`}>
        <circle cx={cx} cy={cy} r={pr} fill="var(--color-ink)" />
        <circle cx={cx - pr * 0.36} cy={cy - pr * 0.38} r={pr * 0.24} fill="#fff" opacity="0.9" />
      </g>
    </g>
  );
}

export type BuddyShape = "keycap" | "esc" | "pill" | "round";

/**
 * A googly-eyed keycap character. Decorative: hidden from assistive tech, never takes focus.
 * `look` is the resting gaze (-1..1 per axis) before the pointer moves.
 */
export function Buddy({
  shape,
  color,
  size,
  rotate = 0,
  look = [0, 0],
  blinkDelay = 0,
  floatDelay = 0,
  className = "",
  style,
}: {
  shape: BuddyShape;
  color: string;
  size: number;
  rotate?: number;
  look?: [number, number];
  blinkDelay?: number;
  floatDelay?: number;
  className?: string;
  style?: CSSProperties;
}) {
  const body = useRef<HTMLDivElement>(null);

  useEffect(() => {
    let last = 0;
    const hop = () => {
      const el = body.current;
      const now = performance.now();
      if (!el || now - last < 140 || el.offsetParent === null) {
        return;
      }
      last = now;
      el.classList.remove("buddy-hop");
      void el.offsetWidth; // Restart the animation.
      el.classList.add("buddy-hop");
    };
    window.addEventListener(PRESS_EVENT, hop);
    return () => window.removeEventListener(PRESS_EVENT, hop);
  }, []);

  const tall = shape === "pill";
  const eye = { look, turn: rotate, blinkDelay };
  return (
    <div
      aria-hidden="true"
      className={`pointer-events-none absolute animate-float select-none ${className}`}
      style={{ animationDelay: `${floatDelay}s`, ...style }}
    >
      <div ref={body} style={{ rotate: `${rotate}deg` }}>
        <svg
          width={size}
          height={tall ? size * 1.15 : size}
          viewBox={tall ? "0 0 200 230" : "0 0 200 200"}
          className="drop-shadow-[0_10px_0_rgba(11,11,11,0.12)]"
        >
          {shape === "keycap" && (
            <>
              <rect x="8" y="8" width="184" height="184" rx="46" fill={color} />
              <rect x="26" y="18" width="148" height="140" rx="36" fill="#fff" opacity="0.14" />
              <GooglyEye cx={80} cy={84} r={38} {...eye} />
              <GooglyEye cx={132} cy={114} r={38} {...eye} />
            </>
          )}
          {shape === "esc" && (
            <>
              <rect x="8" y="8" width="184" height="184" rx="40" fill={color} />
              <rect x="24" y="18" width="152" height="142" rx="30" fill="#fff" opacity="0.08" />
              <text x="38" y="56" fill="#fff" fontSize="26" fontWeight="700" style={{ fontFamily: "var(--font-mono)" }}>
                esc
              </text>
              <GooglyEye cx={112} cy={112} r={50} {...eye} />
            </>
          )}
          {shape === "pill" && (
            <>
              <rect x="14" y="8" width="172" height="214" rx="86" fill={color} />
              <GooglyEye cx={100} cy={80} r={44} {...eye} />
              <GooglyEye cx={100} cy={156} r={44} {...eye} />
            </>
          )}
          {shape === "round" && (
            <>
              <circle cx="100" cy="100" r="92" fill={color} />
              <GooglyEye cx={72} cy={90} r={30} {...eye} />
              <GooglyEye cx={128} cy={90} r={30} {...eye} />
              <path d="M78 136c12 12 32 12 44 0" stroke="var(--color-ink)" strokeWidth="9" strokeLinecap="round" fill="none" />
            </>
          )}
        </svg>
      </div>
    </div>
  );
}
