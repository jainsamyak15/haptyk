// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/*
 * engine-stress: starts 30 voices per second for 10 minutes while switching packs every 10 s;
 * reports callback overruns and memory growth. Owner milestone: M1. Specification: docs/SPEC.md.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char k_usage[] = "usage: engine-stress [--minutes N] [--voices-per-second N]\n";

int main(int argc, char **argv) {
    if (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        fputs(k_usage, stdout);
        return EXIT_SUCCESS;
    }
    /* TODO(M1): implement. */
    fputs("engine-stress: not implemented yet (M1)\n", stderr);
    return 2;
}
