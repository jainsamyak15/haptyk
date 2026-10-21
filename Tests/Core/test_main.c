// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/* Runs every test in kf_test_list.h. Usage: kf-core-tests [name-filter] */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <kf/kf_time.h>

#include "kf_test.h"

typedef struct kf_test_case {
    const char *name;
    void (*run)(void);
} kf_test_case;

#define KF_REGISTER_TEST(name) {#name, test_##name},
static const kf_test_case k_tests[] = {KF_TEST_LIST(KF_REGISTER_TEST)};
#undef KF_REGISTER_TEST

static int g_failed_expectations;

void kf_test_fail(const char *file, int line, const char *expression) {
    g_failed_expectations++;
    fprintf(stderr, "    %s:%d: expected %s\n", file, line, expression);
}

void kf_test_fail_near(const char *file, int line, const char *expression, double actual,
                       double expected) {
    g_failed_expectations++;
    fprintf(stderr, "    %s:%d: %s = %.9g, expected %.9g\n", file, line, expression, actual,
            expected);
}

int main(int argc, char **argv) {
    const char *filter = argc > 1 ? argv[1] : NULL;
    kf_time_init();

    int run = 0;
    int failed = 0;
    for (size_t i = 0; i < sizeof k_tests / sizeof k_tests[0]; ++i) {
        if (filter != NULL && strstr(k_tests[i].name, filter) == NULL) {
            continue;
        }
        const int before = g_failed_expectations;
        k_tests[i].run();
        run++;
        const bool passed = g_failed_expectations == before;
        if (!passed) {
            failed++;
        }
        printf("  %s  %s\n", passed ? "ok  " : "FAIL", k_tests[i].name);
    }
    printf("\n%d tests, %d failed\n", run, failed);
    return failed == 0 && run > 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
