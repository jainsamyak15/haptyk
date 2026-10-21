// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/**
 * @file kf_rng.h
 * @brief Tiny xorshift32 generator for picking sample takes. RT-safe; not for security.
 */
#ifndef KF_RNG_H
#define KF_RNG_H

#include <stdint.h>

#define KF_RNG_NONE UINT32_MAX

typedef struct kf_rng {
    uint32_t state;
} kf_rng;

static inline void kf_rng_seed(kf_rng *rng, uint32_t seed) {
    rng->state = seed != 0u ? seed : 0x9E3779B9u;
}

static inline uint32_t kf_rng_next(kf_rng *rng) {
    uint32_t x = rng->state;
    x ^= x << 13u;
    x ^= x >> 17u;
    x ^= x << 5u;
    rng->state = x;
    return x;
}

/** Uniform in [0, n) without division (Lemire's multiply-shift). n must be > 0. */
static inline uint32_t kf_rng_below(kf_rng *rng, uint32_t n) {
    return (uint32_t)(((uint64_t)kf_rng_next(rng) * (uint64_t)n) >> 32u);
}

/** Uniform in [0, n), never equal to `last` when n > 1. Pass KF_RNG_NONE for no history. */
static inline uint32_t kf_rng_pick_avoiding(kf_rng *rng, uint32_t n, uint32_t last) {
    if (n <= 1u) {
        return 0u;
    }
    if (last >= n) {
        return kf_rng_below(rng, n);
    }
    const uint32_t pick = kf_rng_below(rng, n - 1u);
    return pick >= last ? pick + 1u : pick;
}

#endif /* KF_RNG_H */
