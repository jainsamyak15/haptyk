#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-Proprietary
# Copyright (c) 2026 The Keyfeel Authors. All rights reserved.
#
# Checks the toolchain and creates the repo-local Python environment.
# Changes nothing outside this repository: no sudo, no global installs.
set -euo pipefail
cd "$(dirname "$0")/.."

ok() { printf '  \033[32mok\033[0m    %s\n' "$1"; }
warn() { printf '  \033[33mwarn\033[0m  %s\n' "$1"; }
fail() {
    printf '  \033[31mfail\033[0m  %s\n' "$1"
    exit 1
}

echo "Keyfeel bootstrap"

if [[ "$(uname -s)" == "Darwin" ]]; then
    ok "macOS $(sw_vers -productVersion)"
    if [[ "$(uname -m)" == "arm64" ]]; then
        ok "Apple Silicon"
    else
        warn "not Apple Silicon: the motion sensor and arm64 builds need an M-series Mac"
    fi
    if xcode-select -p >/dev/null 2>&1; then
        ok "Xcode tools at $(xcode-select -p)"
    else
        fail "Xcode Command Line Tools missing. Install them yourself: xcode-select --install"
    fi
else
    warn "$(uname -s): only the portable core and its tests build here"
fi

for tool in clang-format clang-tidy; do
    if command -v "$tool" >/dev/null; then
        ok "$tool"
    else
        warn "$tool not found: make format / make lint need it"
    fi
done

python=""
for candidate in python3.14 python3.13 python3.12 python3.11 python3.10 python3; do
    if command -v "$candidate" >/dev/null &&
        "$candidate" -c 'import sys; sys.exit(0 if sys.version_info >= (3, 10) else 1)'; then
        python="$candidate"
        break
    fi
done
[[ -n "$python" ]] || fail "Python 3.10+ not found. Install one yourself (python.org or Homebrew), then rerun."
ok "$("$python" --version)"

if [[ ! -x Tools/py/.venv/bin/python ]]; then
    "$python" -m venv Tools/py/.venv
fi
Tools/py/.venv/bin/python -m pip install --quiet --upgrade pip
Tools/py/.venv/bin/python -m pip install --quiet -r Tools/py/requirements.txt
ok "Python packages installed in Tools/py/.venv"

echo "Next: make test"
