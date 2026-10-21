#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-Proprietary
# Copyright (c) 2026 The Keyfeel Authors. All rights reserved.
#
# Builds, signs, notarizes and packages a release. Implemented in M5 (docs/RELEASING.md).
# Credentials come from the keychain profile "keyfeel-notary"; nothing secret lives in the repo.
set -euo pipefail
cd "$(dirname "$0")/.."

# TODO(M5):
#   1. xcodebuild archive -scheme Keyfeel -configuration Release (arm64)
#   2. xcodebuild -exportArchive with method developer-id
#   3. ditto -c -k the app; xcrun notarytool submit --keychain-profile keyfeel-notary --wait
#   4. xcrun stapler staple Keyfeel.app
#   5. Build the DMG (hdiutil), sign it, notarize it, staple it
#   6. Run Sparkle's generate_appcast over the releases folder
#   7. Fail if the DMG is larger than 30 MB
echo "release.sh: not implemented until M5 (docs/RELEASING.md)" >&2
exit 2
