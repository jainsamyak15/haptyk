# Releasing (Milestone 5)

Releases are Developer ID–signed, notarized DMGs, updated through Sparkle 2.
`scripts/release.sh` automates these steps once M5 lands.

## One-time setup (owner)

1. Join the Apple Developer Program and create a Developer ID Application certificate.
2. Store notarization credentials in the keychain:
   `xcrun notarytool store-credentials keyfeel-notary` (never commit them).
3. Generate Sparkle's EdDSA keys with Sparkle's `generate_keys`; put the public key in
   `App/Resources/Info.plist` (`SUPublicEDKey`). The private key never enters the repo.
4. Host the appcast at the URL in `SUFeedURL`.

## Every release

1. Update `MARKETING_VERSION` and `CURRENT_PROJECT_VERSION` in `Config/App.xcconfig` and move
   `CHANGELOG.md`'s Unreleased section under the new version.
2. `make ci` passes; the performance log has this release's numbers.
3. `scripts/release.sh`: archive (Release, arm64), export with `developer-id`, notarize and
   staple the app, build, sign, notarize and staple the DMG, run `generate_appcast`, and fail if
   the DMG exceeds 30 MB.
4. Test the DMG on a clean Mac: install, onboarding, typing, update from the previous version.
5. Tag the release commit `vX.Y.Z`.
