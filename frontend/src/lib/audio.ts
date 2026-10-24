/* Browser-only Web Audio playback for the demo. Every function touches `window` APIs, so call
 * them from event handlers or effects in client components, never during render. */

export type SoundKind = "key" | "space" | "enter" | "backspace";
export type PackSounds = Record<SoundKind, AudioBuffer[]>;

const FILES: Record<SoundKind, readonly string[]> = {
  key: ["key1", "key2", "key3", "key4"],
  space: ["space"],
  enter: ["enter"],
  backspace: ["backspace"],
};

const LEVEL = 0.9;

let ctx: AudioContext | null = null;
let master: GainNode | null = null;
const loaded = new Map<string, Promise<PackSounds>>();
const lastTake = new Map<string, number>();

/** Creates or resumes the audio context. Browsers only allow this from a user gesture. */
export function startAudio(): AudioContext {
  if (!ctx) {
    ctx = new AudioContext({ latencyHint: "interactive" });
    master = ctx.createGain();
    master.gain.value = LEVEL;
    master.connect(ctx.destination);
  }
  if (ctx.state === "suspended") {
    void ctx.resume();
  }
  return ctx;
}

/** Fetches and decodes a pack once; later calls share the same promise. */
export function loadPack(id: string): Promise<PackSounds> {
  const c = startAudio();
  let pending = loaded.get(id);
  if (!pending) {
    pending = Promise.all(
      (Object.keys(FILES) as SoundKind[]).map(async (kind) => {
        const buffers = await Promise.all(
          FILES[kind].map(async (file) => {
            const res = await fetch(`/sounds/${id}/${file}.wav`);
            if (!res.ok) {
              throw new Error(`/sounds/${id}/${file}.wav: HTTP ${res.status}`);
            }
            return c.decodeAudioData(await res.arrayBuffer());
          }),
        );
        return [kind, buffers] as const;
      }),
    ).then((entries) => Object.fromEntries(entries) as PackSounds);
    pending.catch(() => loaded.delete(id)); // Let a later press retry.
    loaded.set(id, pending);
  }
  return pending;
}

export function setMuted(muted: boolean) {
  if (ctx && master) {
    master.gain.setTargetAtTime(muted ? 0 : LEVEL, ctx.currentTime, 0.015);
  }
}

/**
 * Plays one keystroke the way the app does: a take that differs from the previous one, a touch
 * of pitch and level variation so repeats never sound robotic, and a pan matching the key's
 * place on the keyboard (-1 left .. 1 right). `quiet` is for modifier keys.
 */
export function playKey(id: string, sounds: PackSounds, kind: SoundKind, pan: number, quiet = false) {
  if (!ctx || !master) {
    return;
  }
  const takes = sounds[kind];
  const slot = `${id}/${kind}`;
  let take = 0;
  if (takes.length > 1) {
    const last = lastTake.get(slot) ?? -1;
    take = Math.floor(Math.random() * (last < 0 ? takes.length : takes.length - 1));
    if (last >= 0 && take >= last) {
      take += 1;
    }
    lastTake.set(slot, take);
  }
  const source = ctx.createBufferSource();
  source.buffer = takes[take];
  source.playbackRate.value = 0.97 + Math.random() * 0.06;
  const gain = ctx.createGain();
  gain.gain.value = (quiet ? 0.55 : 1) * (0.86 + Math.random() * 0.14);
  const panner = ctx.createStereoPanner();
  panner.pan.value = Math.max(-1, Math.min(1, pan));
  source.connect(gain).connect(panner).connect(master);
  source.onended = () => panner.disconnect();
  source.start();
}

/** Loads `id` if needed, then plays one take: for buttons that make a sound on their first
 * press, before the demo has loaded anything. */
export async function hit(id: string, kind: SoundKind, pan = 0) {
  startAudio();
  try {
    playKey(id, await loadPack(id), kind, pan);
  } catch {
    // A missing or undecodable file only costs this one sound.
  }
}
