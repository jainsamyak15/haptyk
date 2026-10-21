// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#include <math.h>

#include <kf/kf_velocity.h>

#include "kf_test.h"

void test_autoadapt_moves_toward_percentiles(void) {
    kf_calibration cal = kf_calibration_default(); /* soft 0.01 g, slam 0.2 g */
    static kf_autoadapt aa;
    kf_autoadapt_init(&aa, &cal);
    bool changed = false;
    for (int i = 0; i < 640; ++i) { /* Uniform 0.012 .. 0.3 g. */
        const float peak = 0.012f + 0.288f * (float)(i % 64) / 63.0f;
        changed = kf_autoadapt_observe(&aa, peak, &cal) || changed;
    }
    KF_EXPECT(changed);
    KF_EXPECT(cal.soft_g > 0.01f);
    KF_EXPECT(cal.soft_g <= 0.02f + 1e-6f); /* Capped at 2x the calibrated value. */
    KF_EXPECT(cal.slam_g > 0.2f && cal.slam_g < 0.3f);
}

void test_autoadapt_respects_clamp(void) {
    kf_calibration cal = kf_calibration_default();
    static kf_autoadapt aa;
    kf_autoadapt_init(&aa, &cal);
    for (int i = 0; i < 6400; ++i) {
        (void)kf_autoadapt_observe(&aa, 5.0f, &cal);
    }
    KF_EXPECT_NEAR(cal.soft_g, 0.02, 1e-6);
    KF_EXPECT_NEAR(cal.slam_g, 0.4, 1e-6);
}

void test_autoadapt_disabled_keeps_calibration(void) {
    kf_calibration cal = kf_calibration_default();
    cal.auto_adapt = false;
    static kf_autoadapt aa;
    kf_autoadapt_init(&aa, &cal);
    for (int i = 0; i < 1000; ++i) {
        KF_EXPECT(!kf_autoadapt_observe(&aa, 0.15f, &cal));
    }
    KF_EXPECT(cal.soft_g == 0.01f && cal.slam_g == 0.2f); /* Bit-for-bit unchanged. */
}

/* Rows farther from the sensor read weaker: factor per row 0..4. */
static const float k_row_factor[5] = {0.6f, 0.8f, 1.0f, 1.25f, 1.5f};

static void make_samples(kf_calibration_sample *soft, kf_calibration_sample *hard) {
    for (int i = 0; i < 50; ++i) {
        const uint8_t row = (uint8_t)(i % 5);
        soft[i].row = row;
        soft[i].peak_g = 0.02f * k_row_factor[row];
        hard[i].row = row;
        hard[i].peak_g = 0.3f * k_row_factor[row];
    }
}

void test_calibration_compute_fits_rows(void) {
    kf_calibration_sample soft[50];
    kf_calibration_sample hard[50];
    make_samples(soft, hard);
    kf_calibration cal = kf_calibration_default();
    cal.auto_adapt = false;
    KF_EXPECT(kf_calibration_compute(0.003f, soft, 50u, hard, 50u, &cal) == KF_OK);
    KF_EXPECT(kf_calibration_is_valid(&cal));
    KF_EXPECT(!cal.auto_adapt); /* Preserved. */
    KF_EXPECT_NEAR(cal.noise_floor_g, 0.003, 1e-7);
    KF_EXPECT_NEAR(cal.soft_g, 0.02, 1e-5);
    KF_EXPECT_NEAR(cal.slam_g, 0.3, 1e-5);
    for (int row = 0; row < 5; ++row) {
        KF_EXPECT_NEAR(cal.row_gain[row], 1.0f / k_row_factor[row], 1e-4);
    }
    KF_EXPECT(cal.row_gain[KF_ROW_FUNCTION] == 1.0f); /* No samples: unchanged gain. */
}

void test_calibration_compute_rejects_bad_input(void) {
    kf_calibration_sample soft[50];
    kf_calibration_sample hard[50];
    make_samples(soft, hard);
    kf_calibration cal = kf_calibration_default();

    KF_EXPECT(kf_calibration_compute(0.003f, soft, 5u, hard, 50u, &cal) == KF_ERR_INVALID_ARGUMENT);
    KF_EXPECT(kf_calibration_compute(0.003f, soft, 50u, soft, 50u, &cal) ==
              KF_ERR_INVALID_ARGUMENT); /* Did not separate. */
    KF_EXPECT(kf_calibration_compute(0.5f, soft, 50u, hard, 50u, &cal) ==
              KF_ERR_INVALID_ARGUMENT); /* Soft taps under the noise floor. */
    KF_EXPECT(kf_calibration_compute(0.003f, soft, 50u, hard, 50u, NULL) ==
              KF_ERR_INVALID_ARGUMENT);

    hard[3].row = 9u;
    KF_EXPECT(kf_calibration_compute(0.003f, soft, 50u, hard, 50u, &cal) ==
              KF_ERR_INVALID_ARGUMENT);
    hard[3].row = 1u;
    hard[4].peak_g = NAN;
    KF_EXPECT(kf_calibration_compute(0.003f, soft, 50u, hard, 50u, &cal) ==
              KF_ERR_INVALID_ARGUMENT);
    KF_EXPECT(cal.soft_g == 0.01f); /* Untouched on failure. */
}
