// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/*
 * kf-cli: plays the active pack while you type, for testing the pipeline without the app;
 * --calibrate runs the calibration math (M2). Owner milestone: M1. Specification: docs/SPEC.md.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char k_usage[] = "usage: kf-cli [--pack DIR] [--calibrate] [--seconds N]\n";

int main(int argc, char **argv) {
    if (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        fputs(k_usage, stdout);
        return EXIT_SUCCESS;
    }
    /* TODO(M1): implement. */
    fputs("kf-cli: not implemented yet (M1)\n", stderr);
    return 2;
}
