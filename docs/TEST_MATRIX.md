# Manual test matrix

The spec's manual matrix ("Testing and performance verification") as a checklist. Run each row
on the build under test with `make run-app` (debug) or the release DMG, and record the result,
the Mac and macOS in the table at the end. Take a system snapshot before and after a session
(`python3 Tools/py/system_snapshot.py take logs/snapshots/<name>`, then `... diff A B`): it must
report no change other than Keyfeel's own folder.

Note for ad-hoc signed debug builds: macOS ties Input Monitoring to the code signature, which
changes on every build, so grant it again after rebuilding (System Settings > Privacy & Security
> Input Monitoring).

## System (M3)

- [ ] A password field (e.g. Safari's) stays silent: Secure Input means Keyfeel receives nothing.
- [ ] Sleep mid-typing, wake, type: sounds return; no sound is left playing across sleep.
- [ ] Lock the screen, unlock, type: sounds return.
- [ ] Fast user switching away and back: sounds return; nothing plays for the other user.
- [ ] Low Power Mode on: typing still plays; CPU stays under 1% at 600 keys a minute.
- [ ] 30 s after typing stops, `pmset -g assertions` lists nothing from Keyfeel, and Activity
      Monitor shows 0.0% CPU.

## Changes (M3)

- [ ] Switch output (speakers, wired headphones, a USB DAC, AirPods, an HDMI display) while
      typing: sound follows within a second, at the right pitch.
- [ ] Change the output device's sample rate in Audio MIDI Setup *only on a test Mac*: Keyfeel
      restarts at the new rate (never do this on the owner's daily Mac: Safety rules).
- [ ] Switch packs and volume while typing: no clicks or gaps.
- [ ] Toggle sounds off and on from the menu: instant.

## Permissions (M3)

- [ ] Fresh install: onboarding appears; "Ask Me" shows the system prompt; after granting,
      sounds start (or "Relaunch Keyfeel" appears and works).
- [ ] Input Monitoring denied, then granted in System Settings while onboarding is open: it
      notices within a second.
- [ ] Revoke Input Monitoring while running: the menu bar icon shows the dot and the menu offers
      "Grant Input Monitoring…".
- [ ] Turning on Shout Mode asks for Accessibility; nothing else ever asks.
- [ ] The microphone is never requested: no microphone prompt at any point.

## Calls (M3)

- [ ] Start a FaceTime or Zoom call: sounds mute; end it: sounds return at once.
- [ ] With a USB headset as both input and output: same (macOS 14+ checks per process).

## Keyboards and load (M6)

- [ ] External keyboard connected, typing on the built-in one: force line says
      "Off (external keyboard)"; sounds play.
- [ ] Typing on the external keyboard with "Mute when typing on an external keyboard": silent.
- [ ] 1,000 keys a minute bursts and a held key: no dropped or late sounds; repeats only with
      "Play sounds for repeating keys".

## Results

| Date | Build | Mac | macOS | Rows passed | Notes |
| --- | --- | --- | --- | --- | --- |
