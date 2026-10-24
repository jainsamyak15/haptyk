import type { InstallStep } from "@/lib/content";

/* Small drawings of the macOS screens a first install meets, for /download. They show macOS's
 * own wording, so they live here rather than in the page copy, and both designs share them: they
 * use fixed macOS colours, not the page's. The orange ring marks what to click. */

const ring = "ring-[3px] ring-[#ff6b1f] ring-offset-2 ring-offset-[#2a2a2c]";

function Cursor({ className }: { className: string }) {
  return (
    <svg width="22" height="22" viewBox="0 0 24 24" aria-hidden="true" className={`absolute drop-shadow ${className}`}>
      <path d="M5 3l14 8-6.2 1.5L16 20l-2.6 1.2-3.2-7.4L5 18z" fill="#fff" stroke="#000" strokeWidth="1.3" strokeLinejoin="round" />
    </svg>
  );
}

function KeyfeelIcon({ size }: { size: number }) {
  return (
    <svg width={size} height={size} viewBox="0 0 48 48" aria-hidden="true">
      <rect width="48" height="48" rx="12" fill="#b4ad9f" />
      <rect x="4" y="3" width="40" height="36" rx="9" fill="#ebe6dc" />
      <path d="M17 11v19M17 21.5l11-10.5M21.5 18l7.5 12" stroke="#121211" strokeWidth="3.8" strokeLinecap="round" strokeLinejoin="round" fill="none" />
      <circle cx="37" cy="31" r="3.2" fill="#ff6b1f" />
    </svg>
  );
}

function Blocked() {
  return (
    <div className="relative w-[250px] max-w-full rounded-[20px] bg-[#2a2a2c] p-5 text-[#f5f5f7] shadow-[0_20px_50px_rgba(0,0,0,0.45)] ring-1 ring-white/10">
      <svg width="46" height="42" viewBox="0 0 46 42" aria-hidden="true">
        <path d="M23 3L44 39H2z" fill="#ffcc00" stroke="#e0a800" strokeWidth="2" strokeLinejoin="round" />
        <path d="M23 15v12" stroke="#2a2a2c" strokeWidth="4" strokeLinecap="round" />
        <circle cx="23" cy="33" r="2.4" fill="#2a2a2c" />
      </svg>
      <p className="mt-3 text-[14px] leading-snug font-bold">“Keyfeel.dmg” Not Opened</p>
      <p className="mt-1.5 text-[11.5px] leading-snug text-[#c7c7cc]">
        Apple could not verify “Keyfeel.dmg” is free of malware that may harm your Mac or compromise your privacy.
      </p>
      <div className="mt-4 grid grid-cols-2 gap-2.5 text-[12.5px] font-medium">
        <span className={`rounded-full bg-[#48484a] py-1.5 text-center ${ring}`}>Done</span>
        <span className="rounded-full bg-[#0a84ff] py-1.5 text-center opacity-40">Move to Bin</span>
      </div>
      <Cursor className="bottom-1 left-[68px]" />
    </div>
  );
}

function Settings() {
  const items = ["General", "Appearance", "Privacy & Security", "Desktop & Dock"];
  return (
    <div className="relative flex w-[340px] max-w-full overflow-hidden rounded-[16px] bg-[#1e1e20] text-[#f5f5f7] shadow-[0_20px_50px_rgba(0,0,0,0.45)] ring-1 ring-white/10">
      <div className="w-[128px] shrink-0 space-y-1 bg-[#2a2a2c] p-2 pt-8 text-[10.5px]">
        {items.map((it) => (
          <p key={it} className={`truncate rounded-[6px] px-2 py-1 ${it === "Privacy & Security" ? "bg-[#0a84ff] font-semibold" : "text-[#c7c7cc]"}`}>
            {it}
          </p>
        ))}
      </div>
      <div className="flex-1 p-3.5 pt-4">
        <p className="text-[12px] font-bold">Privacy & Security</p>
        <p className="mt-3 text-[9.5px] font-semibold tracking-wide text-[#8e8e93] uppercase">Security</p>
        <div className="mt-1.5 rounded-[9px] bg-[#2a2a2c] p-2.5">
          <p className="text-[10.5px] leading-snug text-[#e5e5ea]">“Keyfeel.dmg” was blocked to protect your Mac.</p>
          <div className="mt-2.5 flex justify-end">
            <span className={`rounded-[6px] bg-[#48484a] px-2.5 py-1 text-[10.5px] font-medium ${ring}`}>Open Anyway</span>
          </div>
        </div>
      </div>
      <Cursor className="right-5 bottom-0" />
    </div>
  );
}

function Drag() {
  return (
    <div className="relative w-[320px] max-w-full overflow-hidden rounded-[16px] bg-[#1e1e20] text-[#f5f5f7] shadow-[0_20px_50px_rgba(0,0,0,0.45)] ring-1 ring-white/10">
      <div className="flex items-center gap-1.5 bg-[#2a2a2c] px-3 py-2">
        {["#ff5f57", "#febc2e", "#28c840"].map((c) => (
          <span key={c} className="h-2.5 w-2.5 rounded-full" style={{ background: c }} />
        ))}
        <span className="flex-1 pr-8 text-center text-[11px] font-semibold text-[#c7c7cc]">Keyfeel</span>
      </div>
      <div className="flex items-center justify-between px-8 py-6">
        <div className="flex flex-col items-center gap-1.5 text-[11px]">
          <KeyfeelIcon size={58} />
          Keyfeel
        </div>
        <svg width="80" height="20" viewBox="0 0 80 20" aria-hidden="true">
          <path d="M4 10h66" stroke="#ff6b1f" strokeWidth="2.5" strokeDasharray="5 6" strokeLinecap="round" />
          <path d="M64 3l9 7-9 7" stroke="#ff6b1f" strokeWidth="2.5" fill="none" strokeLinecap="round" strokeLinejoin="round" />
        </svg>
        <div className="flex flex-col items-center gap-1.5 text-[11px]">
          <svg width="62" height="52" viewBox="0 0 62 52" aria-hidden="true">
            <path d="M3 9a5 5 0 0 1 5-5h15l5 6h26a5 5 0 0 1 5 5v29a5 5 0 0 1-5 5H8a5 5 0 0 1-5-5z" fill="#2f8af5" />
            <path d="M3 17h56v27a5 5 0 0 1-5 5H8a5 5 0 0 1-5-5z" fill="#4aa3ff" />
            <path d="M31 22l-8 18M31 22l8 18M26 34h10" stroke="#e8f3ff" strokeWidth="2.6" strokeLinecap="round" />
          </svg>
          Applications
        </div>
      </div>
    </div>
  );
}

function Allow() {
  return (
    <div className="relative w-[320px] max-w-full rounded-[16px] bg-[#1e1e20] p-4 text-[#f5f5f7] shadow-[0_20px_50px_rgba(0,0,0,0.45)] ring-1 ring-white/10">
      <p className="text-[12px] font-bold">Input Monitoring</p>
      <p className="mt-1 text-[10px] leading-snug text-[#8e8e93]">Allow the apps below to monitor input from your keyboard.</p>
      <div className="mt-3 flex items-center gap-2.5 rounded-[9px] bg-[#2a2a2c] p-2.5">
        <KeyfeelIcon size={26} />
        <span className="flex-1 text-[12px] font-medium">Keyfeel</span>
        <span className={`flex h-[20px] w-[36px] items-center justify-end rounded-full bg-[#0a84ff] p-[2px] ${ring}`}>
          <span className="h-4 w-4 rounded-full bg-white" />
        </span>
      </div>
      <Cursor className="right-3 bottom-0" />
    </div>
  );
}

export function MacMock({ kind }: { kind: InstallStep["mock"] }) {
  return (
    <div aria-hidden="true">
      {kind === "blocked" ? <Blocked /> : kind === "settings" ? <Settings /> : kind === "drag" ? <Drag /> : <Allow />}
    </div>
  );
}
