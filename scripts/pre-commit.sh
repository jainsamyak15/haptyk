#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-Proprietary
# Copyright (c) 2026 The Keyfeel Authors. All rights reserved.
#
# Git pre-commit hook: the fast part of `make ci` (format, core tests, real-time symbols).
# Install with `make hooks`. Bypass only knowingly: `git commit --no-verify`.
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"
make --no-print-directory format-check >/dev/null || {
    echo "pre-commit: not clang-formatted; run 'make format'" >&2
    exit 1
}
make --no-print-directory test >/dev/null || {
    echo "pre-commit: core tests fail; run 'make test'" >&2
    exit 1
}
make --no-print-directory check-rt >/dev/null || {
    echo "pre-commit: a real-time unit imports a banned symbol; run 'make check-rt'" >&2
    exit 1
}
