// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/**
 * @file kf_test.h
 * @brief Minimal, dependency-free test harness for the C core (ADR-0004).
 *
 * Tests are plain functions listed in kf_test_list.h. Failed expectations are
 * reported with file and line; a test fails if any expectation fails.
 */
#ifndef KF_TEST_H
#define KF_TEST_H

#include "kf_test_list.h"

void kf_test_fail(const char *file, int line, const char *expression);
void kf_test_fail_near(const char *file, int line, const char *expression, double actual,
                       double expected);

#define KF_EXPECT(condition)                              \
    do {                                                  \
        if (!(condition)) {                               \
            kf_test_fail(__FILE__, __LINE__, #condition); \
        }                                                 \
    } while (0)

#define KF_EXPECT_NEAR(actual, expected, tolerance)                                      \
    do {                                                                                 \
        const double kf_actual = (double)(actual);                                       \
        const double kf_expected = (double)(expected);                                   \
        const double kf_diff =                                                           \
            kf_actual > kf_expected ? kf_actual - kf_expected : kf_expected - kf_actual; \
        if (!(kf_diff <= (double)(tolerance))) {                                         \
            kf_test_fail_near(__FILE__, __LINE__, #actual, kf_actual, kf_expected);      \
        }                                                                                \
    } while (0)

#define KF_DECLARE_TEST(name) void test_##name(void);
KF_TEST_LIST(KF_DECLARE_TEST)
#undef KF_DECLARE_TEST

#endif /* KF_TEST_H */
