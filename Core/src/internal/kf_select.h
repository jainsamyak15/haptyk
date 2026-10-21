// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/**
 * @file kf_select.h
 * @brief In-place selection (quickselect) and percentiles. RT-safe, allocation-free.
 */
#ifndef KF_SELECT_H
#define KF_SELECT_H

#include <stdint.h>

static inline void kf_swapf(float *a, float *b) {
    const float t = *a;
    *a = *b;
    *b = t;
}

/**
 * Reorders a[0..n) so a[k] holds the k-th smallest value, and returns it.
 * Average O(n), median-of-three pivot. Requires no NaNs. Returns 0 for an empty array
 * and clamps k into range.
 */
static inline float kf_select_kth(float *a, int32_t n, int32_t k) {
    if (n <= 0) {
        return 0.0f;
    }
    if (k < 0) {
        k = 0;
    } else if (k > n - 1) {
        k = n - 1;
    }
    int32_t lo = 0;
    int32_t hi = n - 1;
    while (hi > lo) {
        const int32_t mid = lo + (hi - lo) / 2;
        if (a[mid] < a[lo]) {
            kf_swapf(&a[mid], &a[lo]);
        }
        if (a[hi] < a[lo]) {
            kf_swapf(&a[hi], &a[lo]);
        }
        if (a[hi] < a[mid]) {
            kf_swapf(&a[hi], &a[mid]);
        }
        const float pivot = a[mid];
        int32_t i = lo;
        int32_t j = hi;
        while (i <= j) {
            /* The bounds are implied by the median-of-three invariant; stated for provability. */
            while (i < hi && a[i] < pivot) {
                i++;
            }
            while (j > lo && a[j] > pivot) {
                j--;
            }
            if (i <= j) {
                kf_swapf(&a[i], &a[j]);
                i++;
                j--;
            }
        }
        if (k <= j) {
            hi = j;
        } else if (k >= i) {
            lo = i;
        } else {
            return a[k];
        }
    }
    return a[k];
}

/** Nearest-rank percentile (p in 0..1) of a[0..n), reordering a. Returns 0 when n <= 0. */
static inline float kf_percentile_inplace(float *a, int32_t n, float p) {
    if (n <= 0) {
        return 0.0f;
    }
    const float clamped = p < 0.0f ? 0.0f : (p > 1.0f ? 1.0f : p);
    return kf_select_kth(a, n, (int32_t)(clamped * (float)(n - 1) + 0.5f));
}

#endif /* KF_SELECT_H */
