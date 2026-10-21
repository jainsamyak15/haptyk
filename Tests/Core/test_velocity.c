// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#include <math.h>

#include <kf/kf_velocity.h>

#include "kf_test.h"

#define MS     1000000ULL
#define KEY_MS 500u

static kf_accel_ring g_ring;

/** 1 kHz samples from from_ms to to_ms with constant gravity, plus one spike at spike_ms. */
static void fill(uint64_t from_ms, uint64_t to_ms, const float gravity[3], uint64_t spike_ms,
                 const float spike[3]) {
    kf_accel_ring_reset(&g_ring);
    for (uint64_t t = from_ms; t <= to_ms; ++t) {
        kf_accel_sample s = {t * MS, gravity[0], gravity[1], gravity[2]};
        if (t == spike_ms) {
            s.x += spike[0];
            s.y += spike[1];
            s.z += spike[2];
        }
        kf_accel_ring_push(&g_ring, &s);
    }
}

static const float k_flat[3] = {0.0f, 0.0f, 1.0f};

static float velocity_for_spike(float spike_g, const kf_calibration *cal) {
    const float spike[3] = {spike_g, 0.0f, 0.0f};
    fill(0u, 600u, k_flat, KEY_MS + 2u, spike);
    return kf_velocity_estimate(&g_ring, KEY_MS * MS, KF_ROW_HOME, cal);
}

void test_velocity_needs_enough_data(void) {
    const kf_calibration cal = kf_calibration_default();
    const float none[3] = {0.0f, 0.0f, 0.0f};

    kf_accel_ring_reset(&g_ring);
    KF_EXPECT(kf_velocity_peak_g(&g_ring, KEY_MS * MS, KF_ROW_HOME, &cal) < 0.0f);

    fill(0u, KEY_MS, k_flat, 0u, none); /* Window only half recorded. */
    KF_EXPECT(kf_velocity_peak_g(&g_ring, KEY_MS * MS, KF_ROW_HOME, &cal) < 0.0f);

    fill(KEY_MS - 4u, KEY_MS + 20u, k_flat, 0u, none); /* No baseline before the window. */
    KF_EXPECT(kf_velocity_peak_g(&g_ring, KEY_MS * MS, KF_ROW_HOME, &cal) < 0.0f);

    fill(0u, KEY_MS + 20u, k_flat, 0u, none);
    KF_EXPECT(kf_velocity_peak_g(&g_ring, KEY_MS * MS, KF_ROW_HOME, &cal) >= 0.0f);
}

void test_velocity_below_noise_floor_is_zero(void) {
    const kf_calibration cal = kf_calibration_default();
    KF_EXPECT_NEAR(velocity_for_spike(cal.noise_floor_g * 0.5f, &cal), 0.0, 1e-6);
    KF_EXPECT_NEAR(kf_velocity_from_peak(NAN, &cal), 0.0, 1e-6);
}

void test_velocity_log_mapping(void) {
    const kf_calibration cal = kf_calibration_default();
    KF_EXPECT_NEAR(velocity_for_spike(cal.soft_g, &cal), 0.0, 1e-3);
    KF_EXPECT_NEAR(velocity_for_spike(cal.slam_g, &cal), 1.0, 1e-3);
    KF_EXPECT_NEAR(velocity_for_spike(sqrtf(cal.soft_g * cal.slam_g), &cal), 0.5, 1e-3);
    KF_EXPECT_NEAR(velocity_for_spike(cal.slam_g * 4.0f, &cal), 1.0, 1e-6); /* Clamped. */
}

void test_velocity_baseline_removes_gravity(void) {
    const kf_calibration cal = kf_calibration_default();
    const float tilted[3] = {0.3f, -0.2f, 0.93f};
    const float none[3] = {0.0f, 0.0f, 0.0f};
    const float bump[3] = {0.0f, 0.05f, 0.0f};

    fill(0u, 600u, tilted, 0u, none);
    KF_EXPECT_NEAR(kf_velocity_peak_g(&g_ring, KEY_MS * MS, KF_ROW_HOME, &cal), 0.0, 1e-5);

    fill(0u, 600u, tilted, KEY_MS + 3u, bump);
    KF_EXPECT_NEAR(kf_velocity_peak_g(&g_ring, KEY_MS * MS, KF_ROW_HOME, &cal), 0.05, 1e-4);
}

void test_velocity_row_gain(void) {
    kf_calibration cal = kf_calibration_default();
    cal.row_gain[KF_ROW_BOTTOM] = 2.0f;
    const float bump[3] = {0.05f, 0.0f, 0.0f};
    fill(0u, 600u, k_flat, KEY_MS + 1u, bump);
    KF_EXPECT_NEAR(kf_velocity_peak_g(&g_ring, KEY_MS * MS, KF_ROW_HOME, &cal), 0.05, 1e-4);
    KF_EXPECT_NEAR(kf_velocity_peak_g(&g_ring, KEY_MS * MS, KF_ROW_BOTTOM, &cal), 0.10, 2e-4);
    /* Out-of-range rows fall back to the function-row gain instead of reading past the array. */
    KF_EXPECT_NEAR(kf_velocity_peak_g(&g_ring, KEY_MS * MS, 200u, &cal), 0.05, 1e-4);
}

void test_velocity_rejects_invalid_calibration(void) {
    kf_calibration cal = kf_calibration_default();
    cal.slam_g = cal.soft_g;
    KF_EXPECT(!kf_calibration_is_valid(&cal));
    KF_EXPECT(velocity_for_spike(0.05f, &cal) < 0.0f);

    cal = kf_calibration_default();
    cal.row_gain[KF_ROW_TOP] = 3.0f;
    KF_EXPECT(!kf_calibration_is_valid(&cal));
    cal = kf_calibration_default();
    cal.soft_g = NAN;
    KF_EXPECT(!kf_calibration_is_valid(&cal));
    KF_EXPECT(!kf_calibration_is_valid(NULL));
}

void test_tier_weights_at_key_points(void) {
    int lo = -1;
    float w = -1.0f;
    kf_tier_weights(0.0f, &lo, &w);
    KF_EXPECT(lo == 0);
    KF_EXPECT_NEAR(w, 0.0, 1e-6);
    kf_tier_weights(0.5f, &lo, &w);
    KF_EXPECT(lo == 1);
    KF_EXPECT_NEAR(w, 0.5, 1e-6);
    kf_tier_weights(1.0f, &lo, &w);
    KF_EXPECT(lo == 2);
    KF_EXPECT_NEAR(w, 1.0, 1e-6);
    kf_tier_weights(-3.0f, &lo, &w);
    KF_EXPECT(lo == 0);
    KF_EXPECT_NEAR(w, 0.0, 1e-6);
    kf_tier_weights(7.0f, &lo, &w);
    KF_EXPECT(lo == 2);
    KF_EXPECT_NEAR(w, 1.0, 1e-6);
    kf_tier_weights(NAN, &lo, &w);
    KF_EXPECT(lo == 0);
    KF_EXPECT_NEAR(w, 0.0, 1e-6);
}

void test_tier_mix_is_equal_power(void) {
    for (int i = 0; i <= 100; ++i) {
        const kf_tier_mix mix = kf_tier_mix_for((float)i / 100.0f);
        KF_EXPECT(mix.count >= 1u && mix.count <= 2u);
        float power = 0.0f;
        for (uint8_t v = 0u; v < mix.count && v < 2u; ++v) {
            KF_EXPECT(mix.tier[v] < KF_TIER_COUNT);
            power += mix.gain[v] * mix.gain[v];
        }
        /* Skipping a voice under KF_MIN_VOICE_GAIN loses at most its power. */
        KF_EXPECT(power <= 1.0f + 1e-5f);
        KF_EXPECT(power >= 1.0f - KF_MIN_VOICE_GAIN * KF_MIN_VOICE_GAIN - 1e-5f);
    }
}

void test_tier_mix_skips_quiet_voice(void) {
    const kf_tier_mix soft = kf_tier_mix_for(0.0f);
    KF_EXPECT(soft.count == 1u && soft.tier[0] == KF_TIER_SOFT);
    KF_EXPECT_NEAR(soft.gain[0], 1.0, 1e-6);
    const kf_tier_mix slam = kf_tier_mix_for(1.0f);
    KF_EXPECT(slam.count == 1u && slam.tier[0] == KF_TIER_SLAM);
    const kf_tier_mix half = kf_tier_mix_for(0.5f);
    KF_EXPECT(half.count == 2u && half.tier[0] == KF_TIER_MEDIUM && half.tier[1] == KF_TIER_HARD);
    KF_EXPECT_NEAR(half.gain[0], sqrt(0.5), 1e-6);
}
