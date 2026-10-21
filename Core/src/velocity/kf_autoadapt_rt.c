// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/* Real-time translation unit: only RT-safe code (checked by `make check-rt`). */

#include <kf/kf_velocity.h>

#include <string.h>

#include "internal/kf_rt.h"
#include "internal/kf_select.h"

#define KF_AUTOADAPT_MIN_SPAN 1.05f /* Keep slam_g meaningfully above soft_g. */

static inline float clampf(float value, float lo, float hi) {
    if (value < lo) {
        return lo;
    }
    return value > hi ? hi : value;
}

void kf_autoadapt_init(kf_autoadapt *aa, const kf_calibration *cal) {
    memset(aa, 0, sizeof *aa);
    aa->base_soft_g = cal->soft_g;
    aa->base_slam_g = cal->slam_g;
}

KF_RT_SAFE bool kf_autoadapt_observe(kf_autoadapt *aa, float peak_g, kf_calibration *cal) {
    if (!(peak_g > cal->noise_floor_g)) {
        return false; /* Noise, or NaN. */
    }
    aa->history[aa->next] = peak_g;
    aa->next = (aa->next + 1u) % KF_AUTOADAPT_HISTORY;
    if (aa->count < KF_AUTOADAPT_HISTORY) {
        aa->count++;
    }
    if (!cal->auto_adapt) {
        return false;
    }
    aa->since_update++;
    if (aa->since_update < KF_AUTOADAPT_EVERY) {
        return false;
    }
    aa->since_update = 0u;

    const int32_t n = (int32_t)aa->count;
    memcpy(aa->scratch, aa->history, aa->count * sizeof aa->history[0]);
    const float soft_target = kf_percentile_inplace(aa->scratch, n, KF_AUTOADAPT_SOFT_PCT);
    const float slam_target = kf_percentile_inplace(aa->scratch, n, KF_AUTOADAPT_SLAM_PCT);

    const float soft =
        clampf(cal->soft_g + KF_AUTOADAPT_SMOOTHING * (soft_target - cal->soft_g),
               aa->base_soft_g * KF_AUTOADAPT_MIN_SCALE, aa->base_soft_g * KF_AUTOADAPT_MAX_SCALE);
    const float slam =
        clampf(cal->slam_g + KF_AUTOADAPT_SMOOTHING * (slam_target - cal->slam_g),
               aa->base_slam_g * KF_AUTOADAPT_MIN_SCALE, aa->base_slam_g * KF_AUTOADAPT_MAX_SCALE);
    if (!(slam > soft * KF_AUTOADAPT_MIN_SPAN)) {
        return false;
    }
    cal->soft_g = soft;
    cal->slam_g = slam;
    return true;
}
