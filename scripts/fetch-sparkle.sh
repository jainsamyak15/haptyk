#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-Proprietary
# Copyright (c) 2026 The Keyfeel Authors. All rights reserved.
#
# Downloads Sparkle 2, the app's only third-party runtime dependency (spec: Tech stack), into
# build/vendor/ and checks it against a pinned SHA-256 before anything uses it. Changes nothing
# outside this repository.
#
#   scripts/fetch-sparkle.sh                 uses KF_SPARKLE_VERSION and KF_SPARKLE_SHA256 below
#
# First time: run it with KF_SPARKLE_SHA256 empty. It prints the archive's SHA-256 and stops.
# Compare that hash with the one you get from a second, independent download (or the release
# page), then pin it below and commit: from then on a changed archive is refused.
set -euo pipefail
cd "$(dirname "$0")/.."

KF_SPARKLE_VERSION="${KF_SPARKLE_VERSION:-2.6.4}"
KF_SPARKLE_SHA256="${KF_SPARKLE_SHA256:-}" # Pin after verifying (see above).

url="https://github.com/sparkle-project/Sparkle/releases/download/${KF_SPARKLE_VERSION}/Sparkle-${KF_SPARKLE_VERSION}.tar.xz"
dest="build/vendor/sparkle-${KF_SPARKLE_VERSION}"
mkdir -p build/vendor
scratch="$(mktemp -d build/vendor/download.XXXXXX)"
trap 'rm -rf "$scratch"' EXIT

echo "fetch-sparkle: downloading Sparkle ${KF_SPARKLE_VERSION}"
curl --fail --silent --show-error --location --proto '=https' --tlsv1.2 "$url" \
    --output "$scratch/sparkle.tar.xz"
actual="$(shasum -a 256 "$scratch/sparkle.tar.xz" | cut -d' ' -f1)"
if [[ -z "$KF_SPARKLE_SHA256" ]]; then
    echo "fetch-sparkle: SHA-256 is $actual"
    echo "fetch-sparkle: verify it independently, then pin KF_SPARKLE_SHA256 in this script." >&2
    exit 3
fi
if [[ "$actual" != "$KF_SPARKLE_SHA256" ]]; then
    echo "fetch-sparkle: SHA-256 mismatch: got $actual, pinned $KF_SPARKLE_SHA256. Refusing." >&2
    exit 1
fi
rm -rf "$dest"
mkdir -p "$dest"
tar -xJf "$scratch/sparkle.tar.xz" -C "$dest"
codesign --verify --deep --strict "$dest/Sparkle.framework"
ln -sfn "sparkle-${KF_SPARKLE_VERSION}" build/vendor/sparkle
echo "fetch-sparkle: Sparkle ${KF_SPARKLE_VERSION} verified in $dest"
