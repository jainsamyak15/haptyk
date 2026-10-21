#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Proprietary
# Copyright (c) 2026 The Keyfeel Authors. All rights reserved.
"""Build a .kfpack from licensed source recordings (TODO(M4)).

Reads Packs/sources/<pack-id>/ and writes App/Resources/Packs/<Name>.kfpack/.
It should: split takes on silence; trim; add a 2 ms fade-in; normalise per
tier; with --derive-tiers, generate missing tiers from one take (lower gain and
a gentle low-pass for soft, a slight high-shelf and more gain for slam) and mark
them derived; encode CAF/ALAC with macOS's built-in afconvert; and write
pack.json including source_url, license and attribution. Refuses to build when
SOURCES.md is missing or does not allow commercial use.

Usage: build_pack.py --src Packs/sources/<id> --name NAME --switch-type TYPE
                     --license LICENSE [--source-url URL] [--attribution TEXT]
                     [--derive-tiers] [--out App/Resources/Packs]
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

SWITCH_TYPES = ("linear", "tactile", "clicky", "topre", "buckling", "other")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--src", type=Path, required=True)
    parser.add_argument("--name", required=True)
    parser.add_argument("--switch-type", choices=SWITCH_TYPES, required=True)
    parser.add_argument("--license", required=True)
    parser.add_argument("--source-url", default=None)
    parser.add_argument("--attribution", default="")
    parser.add_argument("--derive-tiers", action="store_true")
    parser.add_argument("--out", type=Path, default=Path("App/Resources/Packs"))
    parser.parse_args(argv)
    print("build_pack.py: not implemented yet (M4)", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main())
