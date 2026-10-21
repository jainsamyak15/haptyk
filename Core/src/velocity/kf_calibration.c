// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#include <kf/kf_velocity.h>

#include <math.h>
#include <stdlib.h>

#include "internal/kf_select.h"

#define KF_CALIBRATION_MIN_SAMPLES     10u
#define KF_CALIBRATION_MIN_ROW_SAMPLES 3
#define KF_CALIBRATION_MIN_SPAN        1.5f /* slam_g must exceed soft_g by this factor. */
#define KF_ROW_GAIN_MIN                0.5f
#define KF_ROW_GAIN_MAX                2.0f

kf_calibration kf_calibration_default(void) {
    kf_calibration cal = {
        .noise_floor_g = 0.002f, /* TBD-M0 */
        .soft_g = 0.010f,        /* TBD-M0 */
        .slam_g = 0.200f,        /* TBD-M0 */
        .auto_adapt = true,
    };
    for (int row = 0; row < KF_ROW_COUNT; ++row) {
        cal.row_gain[row] = 1.0f;
    }
    return cal;
}

bool kf_calibration_is_valid(const kf_calibration *cal) {
    if (cal == NULL || !isfinite(cal->noise_floor_g) || !isfinite(cal->soft_g) ||
        !isfinite(cal->slam_g)) {
        return false;
    }
    if (cal->noise_floor_g < 0.0f || !(cal->soft_g > 0.0f) || !(cal->slam_g > cal->soft_g)) {
        return false;
    }
    for (int row = 0; row < KF_ROW_COUNT; ++row) {
        const float gain = cal->row_gain[row];
        if (!(gain >= KF_ROW_GAIN_MIN && gain <= KF_ROW_GAIN_MAX)) {
            return false;
        }
    }
    return true;
}

static bool samples_are_valid(const kf_calibration_sample *samples, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (!isfinite(samples[i].peak_g) || !(samples[i].peak_g > 0.0f) ||
            samples[i].row >= KF_ROW_COUNT) {
            return false;
        }
    }
    return true;
}

static float clamp_gain(float gain) {
    if (gain < KF_ROW_GAIN_MIN) {
        return KF_ROW_GAIN_MIN;
    }
    return gain > KF_ROW_GAIN_MAX ? KF_ROW_GAIN_MAX : gain;
}

kf_status kf_calibration_compute(float noise_floor_g, const kf_calibration_sample *soft,
                                 size_t soft_count, const kf_calibration_sample *hard,
                                 size_t hard_count, kf_calibration *cal) {
    if (cal == NULL || soft == NULL || hard == NULL || soft_count < KF_CALIBRATION_MIN_SAMPLES ||
        hard_count < KF_CALIBRATION_MIN_SAMPLES || soft_count > INT32_MAX ||
        hard_count > INT32_MAX || !isfinite(noise_floor_g) || noise_floor_g < 0.0f ||
        !samples_are_valid(soft, soft_count) || !samples_are_valid(hard, hard_count)) {
        return KF_ERR_INVALID_ARGUMENT;
    }

    const size_t capacity = soft_count > hard_count ? soft_count : hard_count;
    float *scratch = malloc(capacity * sizeof *scratch);
    if (scratch == NULL) {
        return KF_ERR_NO_MEMORY;
    }

    /* Row gains: bring every row's median hard peak to the overall median. */
    for (size_t i = 0; i < hard_count; ++i) {
        scratch[i] = hard[i].peak_g;
    }
    const float overall_hard = kf_percentile_inplace(scratch, (int32_t)hard_count, 0.5f);

    float gains[KF_ROW_COUNT];
    for (int row = 0; row < KF_ROW_COUNT; ++row) {
        int32_t n = 0;
        for (size_t i = 0; i < hard_count; ++i) {
            if (hard[i].row == row) {
                scratch[n++] = hard[i].peak_g;
            }
        }
        gains[row] = 1.0f;
        if (n >= KF_CALIBRATION_MIN_ROW_SAMPLES) {
            gains[row] = clamp_gain(overall_hard / kf_percentile_inplace(scratch, n, 0.5f));
        }
    }

    for (size_t i = 0; i < soft_count; ++i) {
        scratch[i] = soft[i].peak_g * gains[soft[i].row];
    }
    const float soft_g = kf_percentile_inplace(scratch, (int32_t)soft_count, 0.5f);

    for (size_t i = 0; i < hard_count; ++i) {
        scratch[i] = hard[i].peak_g * gains[hard[i].row];
    }
    const float slam_g = kf_percentile_inplace(scratch, (int32_t)hard_count, 0.95f);
    free(scratch);

    if (!(soft_g > noise_floor_g) || !(slam_g > soft_g * KF_CALIBRATION_MIN_SPAN)) {
        return KF_ERR_INVALID_ARGUMENT; /* Soft and hard taps did not separate. */
    }

    cal->noise_floor_g = noise_floor_g;
    cal->soft_g = soft_g;
    cal->slam_g = slam_g;
    for (int row = 0; row < KF_ROW_COUNT; ++row) {
        cal->row_gain[row] = gains[row];
    }
    return KF_OK;
}
