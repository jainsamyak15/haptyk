# Releasing (Milestone 5)

Releases are Developer ID–signed, notarized DMGs, updated through Sparkle 2. Everything below
runs on the owner's Mac with the owner's accounts; no credential or private key ever enters the
repository.

## One-time setup (owner)

1. Join the Apple Developer Program and create a **Developer ID Application** certificate.
2. Store notarization credentials in the keychain:
   `xcrun notarytool store-credentials keyfeel-notary`.
3. Fetch Sparkle and pin it: run `scripts/fetch-sparkle.sh`, verify the printed SHA-256 against
   an independent download, set `KF_SPARKLE_SHA256` in the script, run it again and commit.
4. Generate Sparkle's EdDSA keys once with `build/vendor/sparkle/bin/generate_keys` (the private
   key stays in your keychain) and put the public key in `App/Resources/Info.plist`
   (`SUPublicEDKey`).
5. Set the real URLs in `App/Resources/Info.plist`: `SUFeedURL` (the appcast), `KFStatsBaseURL`
   (the stats backend, HTTPS), `KFPrivacyPolicyURL` and `KFFeedbackURL`. While they are
   placeholders, updates, telemetry and the percentile download stay off and nothing is sent.
6. Choose the final bundle ID and name (`Config/App.xcconfig`; spec "Open decisions").

## Every release

1. Update `MARKETING_VERSION` and `CURRENT_PROJECT_VERSION` in `Config/App.xcconfig` and move
   `CHANGELOG.md`'s Unreleased section under the new version.
2. `make ci` passes; `docs/PERFORMANCE.md` has this release's numbers; the manual rows of
   `docs/TEST_MATRIX.md` pass.
3. Run:

   ```sh
   KF_SIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)" scripts/release.sh
   ```

   It runs the checks, builds the release app, embeds and signs Sparkle inside-out, signs the
   app with the Hardened Runtime, notarizes and staples it, builds the DMG, signs, notarizes and
   staples that, runs `generate_appcast` over `build/releases/`, and fails if the DMG is larger
   than 30 MB. `scripts/release.sh --local` does the same with an ad-hoc signature and no
   notarization, to check contents and size without credentials.
4. Upload the DMG and the appcast, then test the DMG on a clean Mac: install, onboarding,
   typing, percentile and badge, and an update from the previous version.
5. Tag the release commit `vX.Y.Z`.

## Signing notes

- App Sandbox stays off: the event tap and the sensor need it off (spec).
- `App/Resources/Keyfeel.entitlements` is empty; add only what is proven necessary, with an
  entry in `docs/DECISIONS.md`.
- MetricKit crash and hang reports: verify on the first notarized release that macOS delivers
  them to an app outside the App Store (they appear as files in
  `~/Library/Application Support/Keyfeel/diagnostics/` the day after a crash). If they never do,
  PLCrashReporter is the spec's fallback, which needs a decision record.
