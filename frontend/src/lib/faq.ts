/** Questions both designs answer. Native <details> renders them: keyboard and screen reader
 * support for free, and no JavaScript. Keep the answers in step with the app. */
export const FAQ = [
  {
    q: "Which Macs does Keyfeel run on?",
    a: "Apple Silicon Macs on macOS 13 Ventura or later. It is made for a MacBook's built-in keyboard and plays along with external keyboards too.",
  },
  {
    q: "Why does it ask for Input Monitoring?",
    a: "macOS asks before any app can notice key presses. Keyfeel uses each press only to choose its sound and drops the key code right after. Nothing you type is stored, logged or sent anywhere, and password fields stay silent.",
  },
  {
    q: "Will it slow my Mac down or drain the battery?",
    a: "It does nothing until you type: no timers, no polling. Thirty seconds after your last key, audio stops and it sits at 0.0% CPU. Typing 600 keys a minute stays under 1%.",
  },
  {
    q: "Does it change my sound settings?",
    a: "No. It never touches your volume, output device or sample rate. Its low-latency setting applies to its own audio only.",
  },
  {
    q: "What happens when I'm on a call?",
    a: "When another app starts using your microphone, Keyfeel goes quiet on its own, so nobody hears you clacking. You can turn that off in Settings.",
  },
  {
    q: "macOS says it can't verify Keyfeel. What do I do?",
    a: "Early builds aren't notarized by Apple yet. Click Done, then open System Settings, go to Privacy & Security, scroll down and click Open Anyway. You may need to do this twice, once for the disk image and once for the app, and only the first time.",
  },
  {
    q: "Does it start when I log in?",
    a: "Only if you switch it on in Settings. It stays off until you do.",
  },
  {
    q: "Where do the sounds come from?",
    a: "Real switches, recorded by the keyboard community: Mechvibes and kbsim (MIT licence), Freesound recordings (CC0 and CC BY), and pack authors who gave their permission. Every pack is credited in the app.",
  },
  {
    q: "How do I uninstall it?",
    a: "Quit Keyfeel from its menu bar panel and drag it to the Trash. If you want its settings and stats gone too, Settings has a Remove All Keyfeel Data button.",
  },
];
