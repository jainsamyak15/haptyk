// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/**
 * @file kf_velocity.h
 * @brief Keystroke force: peak detection, calibration, tier blending, auto-adapt.
 *
 * Pipeline per key-down: kf_velocity_peak_g() -> kf_velocity_from_peak() ->
 * kf_tier_mix_for(). Everything here except kf_calibration_compute() is RT-safe.
 */
#ifndef KF_VELOCITY_H
#define KF_VELOCITY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <kf/kf_accel.h>
#include <kf/kf_config.h>
#include <kf/kf_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Per-user mapping from measured peak (g) to velocity 0..1. */
typedef struct kf_calibration {
    float noise_floor_g;          /**< Peaks at or below this read as v = 0. */
    float soft_g;                 /**< Row-compensated peak of a typical soft tap (v = 0). */
    float slam_g;                 /**< Row-compensated peak of a typical slam (v = 1). */
    float row_gain[KF_ROW_COUNT]; /**< Distance-from-sensor compensation, 0.5 .. 2.0. */
    bool auto_adapt;              /**< Let kf_autoadapt_observe() refine soft_g and slam_g. */
} kf_calibration;

/** Built-in defaults until the user calibrates. Values are TBD-M0. */
kf_calibration kf_calibration_default(void);

/** True when every field is finite and in range. RT-safe. */
bool kf_calibration_is_valid(const kf_calibration *cal);

/** One raw (not row-compensated) peak collected by the calibration wizard. */
typedef struct kf_calibration_sample {
    float peak_g;
    uint8_t row; /**< kf_key_row. */
} kf_calibration_sample;

/**
 * Fits a calibration from wizard samples (spec: Core C modules, velocity.c).
 * Row gains come from per-row hard medians; soft_g is the median soft peak and
 * slam_g the 95th-percentile hard peak, both row-compensated. Needs at least 10
 * samples of each kind. On success writes every field except auto_adapt.
 * Not RT-safe (allocates). Returns KF_ERR_INVALID_ARGUMENT when the input is bad
 * or soft and hard taps did not separate.
 */
kf_status kf_calibration_compute(float noise_floor_g, const kf_calibration_sample *soft,
                                 size_t soft_count, const kf_calibration_sample *hard,
                                 size_t hard_count, kf_calibration *cal);

/**
 * Row-compensated peak (g) of the motion around a key-down, with gravity and
 * tilt removed by a baseline taken just before the window. Returns -1 when the
 * ring lacks baseline samples or holds under half the expected window samples.
 * RT-safe; O(window + baseline).
 */
float kf_velocity_peak_g(const kf_accel_ring *ring, uint64_t t_key_ns, uint8_t row,
                         const kf_calibration *cal);

/** Log-scale mapping of a peak to 0..1. Requires a valid calibration. RT-safe. */
float kf_velocity_from_peak(float peak_g, const kf_calibration *cal);

/** kf_velocity_peak_g() then kf_velocity_from_peak(); -1 when unmeasurable. RT-safe. */
float kf_velocity_estimate(const kf_accel_ring *ring, uint64_t t_key_ns, uint8_t row,
                           const kf_calibration *cal);

/** Splits velocity across the four tiers: tier lo gets (1 - w), tier lo + 1 gets w. RT-safe. */
void kf_tier_weights(float velocity, int *lo, float *w);

/** Up to two tier voices with equal-power gains; voices under KF_MIN_VOICE_GAIN are skipped. */
typedef struct kf_tier_mix {
    uint8_t count;   /**< 1 or 2. */
    uint8_t tier[2]; /**< kf_tier. */
    float gain[2];   /**< Equal-power gains before the overall level. */
} kf_tier_mix;

/** RT-safe. */
kf_tier_mix kf_tier_mix_for(float velocity);

/** Slow, bounded refinement of soft_g and slam_g from the user's own typing. */
typedef struct kf_autoadapt {
    float history[KF_AUTOADAPT_HISTORY]; /**< Ring of recent row-compensated peaks. */
    float scratch[KF_AUTOADAPT_HISTORY]; /**< Preallocated work area for percentiles. */
    uint32_t count;                      /**< Valid entries in history. */
    uint32_t next;                       /**< Next write index. */
    uint32_t since_update;               /**< Keystrokes since the last re-fit. */
    float base_soft_g;                   /**< Calibrated anchors that bound adaptation. */
    float base_slam_g;
} kf_autoadapt;

/** Starts adapting from `cal`. Not thread-safe. */
void kf_autoadapt_init(kf_autoadapt *aa, const kf_calibration *cal);

/**
 * Records one row-compensated peak; every KF_AUTOADAPT_EVERY keystrokes moves
 * soft_g and slam_g a small step toward the 10th and 97th percentiles, within
 * 0.5x..2x of the anchors. Returns true when `cal` changed. RT-safe.
 */
bool kf_autoadapt_observe(kf_autoadapt *aa, float peak_g, kf_calibration *cal);

#ifdef __cplusplus
}
#endif

#endif /* KF_VELOCITY_H */
