// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/**
 * @file kf_accel.h
 * @brief Motion-sensor samples and the history ring the velocity math reads.
 */
#ifndef KF_ACCEL_H
#define KF_ACCEL_H

#include <stdint.h>

#include <kf/kf_config.h>
#include <kf/kf_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** One decoded motion-sensor reading, in g. */
typedef struct kf_accel_sample {
    uint64_t t_ns; /**< Reading time, ns on the kf_time clock. */
    float x;
    float y;
    float z;
} kf_accel_sample;

KF_STATIC_ASSERT((KF_ACCEL_RING_SAMPLES & (KF_ACCEL_RING_SAMPLES - 1u)) == 0u,
                 "KF_ACCEL_RING_SAMPLES must be a power of two");

/**
 * Fixed-size history of recent samples, pushed in time order.
 * Owned by the sensor thread; not thread-safe.
 */
typedef struct kf_accel_ring {
    kf_accel_sample samples[KF_ACCEL_RING_SAMPLES];
    uint64_t written; /**< Total samples ever pushed. */
} kf_accel_ring;

static inline void kf_accel_ring_reset(kf_accel_ring *ring) {
    ring->written = 0u;
}

/** Appends a sample, overwriting the oldest when full. RT-safe. */
static inline void kf_accel_ring_push(kf_accel_ring *ring, const kf_accel_sample *sample) {
    ring->samples[ring->written & (KF_ACCEL_RING_SAMPLES - 1u)] = *sample;
    ring->written++;
}

/** Number of samples currently held. RT-safe. */
static inline uint32_t kf_accel_ring_size(const kf_accel_ring *ring) {
    return ring->written < KF_ACCEL_RING_SAMPLES ? (uint32_t)ring->written : KF_ACCEL_RING_SAMPLES;
}

/** Sample `age` steps back from the newest (0 = newest). Requires age < size. RT-safe. */
static inline const kf_accel_sample *kf_accel_ring_at_age(const kf_accel_ring *ring, uint32_t age) {
    return &ring->samples[(ring->written - 1u - age) & (KF_ACCEL_RING_SAMPLES - 1u)];
}

#ifdef __cplusplus
}
#endif

#endif /* KF_ACCEL_H */
