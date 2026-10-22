#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-Proprietary
# Copyright (c) 2026 The Keyfeel Authors. All rights reserved.
#
# Builds, signs, notarizes and packages a release (docs/RELEASING.md).
#
#   scripts/release.sh           Developer ID signed, notarized and stapled app and DMG
#   scripts/release.sh --local   ad-hoc signed, not notarized: to check contents and size
#
# Full releases need KF_SIGN_IDENTITY ("Developer ID Application: Name (TEAMID)"), a notarytool
# keychain profile (KF_NOTARY_PROFILE, default keyfeel-notary, created once with
# `xcrun notarytool store-credentials`) and Sparkle in build/vendor (scripts/fetch-sparkle.sh).
# Credentials never come from the repository. Writes only under build/.
set -euo pipefail
cd "$(dirname "$0")/.."

local_build=0
if [[ "${1:-}" == "--local" ]]; then
    local_build=1
elif [[ $# -gt 0 ]]; then
    echo "usage: scripts/release.sh [--local]" >&2
    exit 2
fi

readonly max_dmg_bytes=$((30 * 1024 * 1024)) # Spec: the DMG stays under 30 MB.
version="$(sed -n 's/^MARKETING_VERSION = //p' Config/App.xcconfig)"
app="build/release/Keyfeel.app"
out="build/releases"
dmg="$out/Keyfeel-$version.dmg"
profile="${KF_NOTARY_PROFILE:-keyfeel-notary}"
identity="-"
if ((!local_build)); then
    identity="${KF_SIGN_IDENTITY:?set KF_SIGN_IDENTITY to your Developer ID Application identity}"
fi
mkdir -p "$out"

step() { printf '\n== %s\n' "$1"; }

# Hardened Runtime and a secure timestamp for Developer ID; ad-hoc builds have no timestamp.
sign() {
    if [[ "$identity" == "-" ]]; then
        codesign --force --options runtime --timestamp=none --sign - "$@"
    else
        codesign --force --options runtime --timestamp --sign "$identity" "$@"
    fi
}

notarize() {
    xcrun notarytool submit "$1" --keychain-profile "$profile" --wait
}

step "1. Checks and release build"
make --no-print-directory test test-app check-rt
make --no-print-directory CONFIG=release app

step "2. Sparkle and signing"
framework="$app/Contents/Frameworks/Sparkle.framework"
if [[ -d build/vendor/sparkle/Sparkle.framework ]]; then
    mkdir -p "$app/Contents/Frameworks"
    ditto build/vendor/sparkle/Sparkle.framework "$framework"
elif ((!local_build)); then
    echo "release: Sparkle is missing: run scripts/fetch-sparkle.sh" >&2
    exit 1
fi
if [[ -d "$framework" ]]; then
    # Inside out, as Sparkle's documentation asks for apps not signed by Xcode.
    for helper in "$framework"/Versions/B/XPCServices/*.xpc "$framework"/Versions/B/Autoupdate \
        "$framework"/Versions/B/Updater.app; do
        if [[ -e "$helper" ]]; then
            sign "$helper"
        fi
    done
    sign "$framework"
fi
sign --entitlements App/Resources/Keyfeel.entitlements "$app"
codesign --verify --deep --strict "$app"

if ((!local_build)); then
    step "3. Notarize and staple the app"
    ditto -c -k --keepParent "$app" "$out/Keyfeel.zip"
    notarize "$out/Keyfeel.zip"
    xcrun stapler staple "$app"
    rm -f "$out/Keyfeel.zip"
fi

step "4. DMG"
stage="$(mktemp -d build/dmg.XXXXXX)"
trap 'rm -rf "$stage"' EXIT
ditto "$app" "$stage/Keyfeel.app"
ln -s /Applications "$stage/Applications"
rm -f "$dmg"
hdiutil create -volname "Keyfeel" -srcfolder "$stage" -format UDZO -quiet "$dmg"
if [[ "$identity" == "-" ]]; then
    codesign --force --timestamp=none --sign - "$dmg"
else
    codesign --force --timestamp --sign "$identity" "$dmg"
    notarize "$dmg"
    xcrun stapler staple "$dmg"
fi

if ((!local_build)); then
    step "5. Appcast"
    if [[ -x build/vendor/sparkle/bin/generate_appcast ]]; then
        build/vendor/sparkle/bin/generate_appcast "$out" # Uses the EdDSA key in the keychain.
    else
        echo "release: generate_appcast not found; run scripts/fetch-sparkle.sh" >&2
        exit 1
    fi
fi

step "6. Size"
size="$(stat -f %z "$dmg")"
printf 'release: %s is %.1f MB (limit 30 MB)\n' "$dmg" "$(echo "$size / 1048576" | bc -l)"
if ((size > max_dmg_bytes)); then
    echo "release: the DMG is over 30 MB" >&2
    exit 1
fi
