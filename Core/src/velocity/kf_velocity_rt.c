// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/* Real-time translation unit: only RT-safe code (checked by `make check-rt`). */

#include <kf/kf_velocity.h>

#include <math.h>

#include "internal/kf_rt.h"

/** Samples the window should hold at the nominal sensor rate. */
#define KF_WINDOW_EXPECTED_SAMPLES \
    ((uint32_t)(((KF_WINDOW_PRE_NS + KF_WINDOW_POST_NS) * KF_SENSOR_NOMINAL_HZ) / 1000000000ULL))

static inline uint8_t clamp_row(uint8_t row) {
    return row < KF_ROW_COUNT ? row : (uint8_t)KF_ROW_FUNCTION;
}

KF_RT_SAFE float kf_velocity_peak_g(const kf_accel_ring *ring, uint64_t t_key_ns, uint8_t row,
                                    const kf_calibration *cal) {
    const uint64_t window_start = t_key_ns > KF_WINDOW_PRE_NS ? t_key_ns - KF_WINDOW_PRE_NS : 0u;
    const uint64_t window_end = t_key_ns + KF_WINDOW_POST_NS;
    const uint64_t baseline_start =
        window_start > KF_BASELINE_NS ? window_start - KF_BASELINE_NS : 0u;
    const uint32_t size = kf_accel_ring_size(ring);

    /* Pass 1, newest to oldest: baseline mean and the window's extent. */
    float sum_x = 0.0f;
    float sum_y = 0.0f;
    float sum_z = 0.0f;
    uint32_t baseline_count = 0u;
    uint32_t window_count = 0u;
    uint32_t window_newest_age = 0u;
    uint32_t window_oldest_age = 0u;
    for (uint32_t age = 0u; age < size; ++age) {
        const kf_accel_sample *s = kf_accel_ring_at_age(ring, age);
        if (s->t_ns < baseline_start) {
            break;
        }
        if (s->t_ns > window_end) {
            continue;
        }
        if (s->t_ns >= window_start) {
            if (window_count == 0u) {
                window_newest_age = age;
            }
            window_oldest_age = age;
            window_count++;
        } else {
            sum_x += s->x;
            sum_y += s->y;
            sum_z += s->z;
            baseline_count++;
        }
    }
    if (baseline_count == 0u || window_count * 2u < KF_WINDOW_EXPECTED_SAMPLES) {
        return -1.0f;
    }

    /* Pass 2: largest deviation from the baseline inside the window. */
    const float inv = 1.0f / (float)baseline_count;
    const float base_x = sum_x * inv;
    const float base_y = sum_y * inv;
    const float base_z = sum_z * inv;
    float max_sq = 0.0f;
    for (uint32_t age = window_newest_age; age <= window_oldest_age; ++age) {
        const kf_accel_sample *s = kf_accel_ring_at_age(ring, age);
        const float dx = s->x - base_x;
        const float dy = s->y - base_y;
        const float dz = s->z - base_z;
        const float sq = dx * dx + dy * dy + dz * dz;
        if (sq > max_sq) {
            max_sq = sq;
        }
    }
    return sqrtf(max_sq) * cal->row_gain[clamp_row(row)];
}

KF_RT_SAFE float kf_velocity_from_peak(float peak_g, const kf_calibration *cal) {
    if (!(peak_g > cal->noise_floor_g)) {
        return 0.0f; /* Also maps NaN to 0. */
    }
    const float lo = logf(cal->soft_g);
    const float hi = logf(cal->slam_g);
    const float v = (logf(peak_g) - lo) / (hi - lo);
    if (v < 0.0f) {
        return 0.0f;
    }
    return v > 1.0f ? 1.0f : v;
}

KF_RT_SAFE float kf_velocity_estimate(const kf_accel_ring *ring, uint64_t t_key_ns, uint8_t row,
                                      const kf_calibration *cal) {
    if (!kf_calibration_is_valid(cal)) {
        return -1.0f;
    }
    const float peak = kf_velocity_peak_g(ring, t_key_ns, row, cal);
    return peak < 0.0f ? -1.0f : kf_velocity_from_peak(peak, cal);
}

KF_RT_SAFE void kf_tier_weights(float velocity, int *lo, float *w) {
    float v = velocity;
    if (!(v > 0.0f)) {
        v = 0.0f; /* Also maps NaN to 0. */
    }
    if (v > 1.0f) {
        v = 1.0f;
    }
    const float top = (float)(KF_TIER_COUNT - 1);
    const float x = v * top;
    const int low = x >= top ? KF_TIER_COUNT - 2 : (int)x;
    *lo = low;
    *w = x - (float)low;
}

KF_RT_SAFE kf_tier_mix kf_tier_mix_for(float velocity) {
    int lo = 0;
    float w = 0.0f;
    kf_tier_weights(velocity, &lo, &w);
    const float gains[2] = {sqrtf(1.0f - w), sqrtf(w)};

    kf_tier_mix mix = {.count = 0u};
    for (int i = 0; i < 2; ++i) {
        if (gains[i] >= KF_MIN_VOICE_GAIN) {
            mix.tier[mix.count] = (uint8_t)(lo + i);
            mix.gain[mix.count] = gains[i];
            mix.count++;
        }
    }
    return mix;
}
