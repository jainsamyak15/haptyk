// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#include <string.h>

#include <kf/kf_time.h>
#include <kf/kf_types.h>

#include "internal/kf_rng.h"
#include "internal/kf_select.h"
#include "kf_test.h"

void test_status_strings_cover_every_code(void) {
    for (int s = KF_OK; s <= KF_ERR_NOT_IMPLEMENTED; ++s) {
        const char *text = kf_status_string((kf_status)s);
        KF_EXPECT(text != NULL && strcmp(text, "unknown status") != 0);
    }
}

void test_time_conversion_is_exact(void) {
    /* Apple Silicon timebase: 125/3 ns per tick (24 MHz). */
    KF_EXPECT(kf_time_ticks_to_ns_with(24000000u, 125u, 3u) == 1000000000u);
    KF_EXPECT(kf_time_ticks_to_ns_with(3u, 125u, 3u) == 125u);
    KF_EXPECT(kf_time_ticks_to_ns_with(1u, 1u, 1u) == 1u);
    KF_EXPECT(kf_time_ticks_to_ns_with(5u, 1u, 0u) == 0u);
    /* One year of uptime must not overflow. */
    const uint64_t year_ticks = 24000000ULL * 3600ULL * 24ULL * 365ULL;
    KF_EXPECT(kf_time_ticks_to_ns_with(year_ticks, 125u, 3u) ==
              1000000000ULL * 3600ULL * 24ULL * 365ULL);
    const uint64_t a = kf_time_now_ns();
    const uint64_t b = kf_time_now_ns();
    KF_EXPECT(b >= a);
}

void test_rng_stays_in_range(void) {
    kf_rng rng;
    kf_rng_seed(&rng, 42u);
    for (int i = 0; i < 10000; ++i) {
        KF_EXPECT(kf_rng_below(&rng, 7u) < 7u);
    }
    kf_rng_seed(&rng, 0u); /* Zero seed must not lock the generator at zero. */
    KF_EXPECT(kf_rng_next(&rng) != 0u);
}

void test_rng_never_repeats_last(void) {
    kf_rng rng;
    kf_rng_seed(&rng, 7u);
    uint32_t last = KF_RNG_NONE;
    uint32_t seen[3] = {0u, 0u, 0u};
    for (int i = 0; i < 3000; ++i) {
        const uint32_t pick = kf_rng_pick_avoiding(&rng, 3u, last);
        KF_EXPECT(pick < 3u);
        KF_EXPECT(pick != last);
        seen[pick]++;
        last = pick;
    }
    KF_EXPECT(seen[0] > 0u && seen[1] > 0u && seen[2] > 0u);
    KF_EXPECT(kf_rng_pick_avoiding(&rng, 1u, 0u) == 0u);
}

static void insertion_sort(float *a, int n) {
    for (int i = 1; i < n; ++i) {
        const float v = a[i];
        int j = i - 1;
        while (j >= 0 && a[j] > v) {
            a[j + 1] = a[j];
            j--;
        }
        a[j + 1] = v;
    }
}

void test_select_matches_sort(void) {
    kf_rng rng;
    kf_rng_seed(&rng, 1234u);
    float data[512];
    float sorted[512];
    float work[512];
    const int sizes[] = {1, 2, 3, 5, 16, 63, 100, 512};
    for (size_t s = 0; s < sizeof sizes / sizeof sizes[0]; ++s) {
        const int n = sizes[s];
        for (int trial = 0; trial < 20; ++trial) {
            for (int i = 0; i < n; ++i) {
                /* Few distinct values exercise duplicate handling. */
                data[i] = (float)kf_rng_below(&rng, trial % 2 == 0 ? 10u : 100000u);
            }
            memcpy(sorted, data, (size_t)n * sizeof(float));
            insertion_sort(sorted, n);
            for (int k = 0; k < n; k += (n > 20 ? n / 7 : 1)) {
                memcpy(work, data, (size_t)n * sizeof(float));
                KF_EXPECT(kf_select_kth(work, n, k) == sorted[k]);
            }
            memcpy(work, data, (size_t)n * sizeof(float));
            KF_EXPECT(kf_percentile_inplace(work, n, 1.0f) == sorted[n - 1]);
            memcpy(work, data, (size_t)n * sizeof(float));
            KF_EXPECT(kf_percentile_inplace(work, n, 0.0f) == sorted[0]);
        }
    }
}
