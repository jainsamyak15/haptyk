# Privacy

Keyfeel sees which key is pressed only to choose a sound, and feels how hard it was pressed.
Nothing the user types ever leaves the Mac. This document is the engineering reference; the
public privacy policy is written by the owner.

## Never collected, stored or sent

Typed text, key codes, which keys were pressed, window titles, app names, URLs, file names,
microphone audio. The app never opens the microphone; it only reads whether another app is
using it, to mute itself during calls.

## Kept on the Mac

| Data | Where | Retention |
| --- | --- | --- |
| Daily counts: keystrokes, 10-bucket force histogram, typing minutes, peak keys/min | `~/Library/Application Support/Keyfeel/stats/` | 90 days; "Reset Stats" deletes |
| Calibration and preferences | The app's preferences domain | Until "Remove All Keyfeel Data" |
| Crash and hang diagnostics (MetricKit) | Application Support | 30 days |

## Sent, only with the user's opt-in

| Data | Endpoint | Frequency |
| --- | --- | --- |
| Random install ID, app version, Mac model, macOS version, day, keystroke count, force histogram, force mode | `POST /v1/telemetry/daily` | Once a day |
| Crash and hang diagnostics (no key data) | `POST /v1/diagnostics` | With the daily upload |

The percentile table (`GET /v1/stats/percentiles`) is downloaded by every user and sends nothing
about them.

## Enforced in code

- `kf_key_event` has no key-code field (`Core/include/kf/kf_types.h`).
- The event tap respects Secure Input; the app never works around it.
- Shout Mode (Labs, off by default) is the one feature that changes a keystroke: it adds the
  Shift flag to a letter key's key-down, decided from the key code and the force of the last
  three keystrokes. It never reads characters and never touches shortcuts or other keys
  (ADR-0018).
- Key data is logged only in debug builds with `KF_DEBUG_KEYS` enabled.
- The upload payload is built by one function, `+[KFTelemetry payloadForDay:...]`, whose fields
  a unit test pins to exactly the list above.
- Uploads use an ephemeral session (no cookies or cache on disk), only complete past days, and
  only with the opt-in. With a placeholder backend URL in `Info.plist`, nothing is ever sent.
- The badge card is drawn on the Mac; it leaves only if the user shares it.

## Development tools

`Tools/sensor-log` (Milestone 0) runs a listen-only event tap on the developer's own Mac and
records only the time and direction (`KEYDOWN`/`KEYUP`) of each keystroke: never key codes,
characters or the focused app. Its logs go to the git-ignored `logs/` folder.
`Tools/py/m0_protocol.py` discards anything typed into its terminal during a recording.
