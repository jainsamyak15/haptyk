// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/**
 * @file kf_config.h
 * @brief Compile-time tunables for the core. Every tunable lives here.
 *
 * Values marked TBD-M0 are placeholders until Milestone 0 measures them on real
 * hardware; record the measurements and their source in docs/SENSOR_NOTES.md.
 */
#ifndef KF_CONFIG_H
#define KF_CONFIG_H

/* Audio ------------------------------------------------------------------ */
#define KF_SAMPLE_RATE_HZ          48000u       /**< Preferred output rate. */
#define KF_IO_BUFFER_FRAMES        128u         /**< Requested buffer: 2.7 ms at 48 kHz. */
#define KF_MAX_VOICES              24u          /**< Fixed voice pool. */
#define KF_VOICE_STEAL_FADE_FRAMES 96u          /**< 2 ms fade when stealing a voice. */
#define KF_VOICE_QUEUE_CAPACITY    256u         /**< Per-producer queue; power of two. */
#define KF_MIN_VOICE_GAIN          0.05f        /**< Quieter tier voices are skipped. */
#define KF_RELEASE_GAIN            0.6f         /**< Gain of key-up sounds. */
#define KF_QUEUE_MAX_AGE_NS        150000000ULL /**< Drop requests older than 150 ms. */

/* Sensor ----------------------------------------------------------------- */
#define KF_SENSOR_NOMINAL_HZ      1000ULL     /**< TBD-M0: measured sample rate. */
#define KF_ACCEL_RING_SAMPLES     4096u       /**< ~4 s at 1 kHz; power of two. */
#define KF_PENDING_QUEUE_CAPACITY 64u         /**< Keys awaiting sensor data. */
#define KF_SENSOR_STALE_NS        20000000ULL /**< No sample for 20 ms = stale. */
#define KF_PENDING_MAX_AGE_NS     40000000ULL /**< M2 watchdog: default after 40 ms. */

/* Velocity --------------------------------------------------------------- */
#define KF_WINDOW_PRE_NS       5000000ULL  /**< TBD-M0: window start before key-down. */
#define KF_WINDOW_POST_NS      8000000ULL  /**< TBD-M0: window end after key-down. */
#define KF_BASELINE_NS         50000000ULL /**< Baseline span before the window. */
#define KF_DEFAULT_VELOCITY    0.40f       /**< Used when force can't be measured. */
#define KF_AUTOADAPT_HISTORY   512u        /**< Peaks kept for auto-adapt. */
#define KF_AUTOADAPT_EVERY     64u         /**< Re-fit after this many keystrokes. */
#define KF_AUTOADAPT_SMOOTHING 0.1f        /**< Step toward each new estimate. */
#define KF_AUTOADAPT_SOFT_PCT  0.10f       /**< soft_g tracks this percentile. */
#define KF_AUTOADAPT_SLAM_PCT  0.97f       /**< slam_g tracks this percentile. */
#define KF_AUTOADAPT_MIN_SCALE 0.5f        /**< Never below 0.5x the calibration. */
#define KF_AUTOADAPT_MAX_SCALE 2.0f        /**< Never above 2x the calibration. */

/* Packs ------------------------------------------------------------------ */
#define KF_PACK_MAX_FILES         400u
#define KF_PACK_MAX_DECODED_BYTES (8u * 1024u * 1024u)
#define KF_PACK_MAX_FILE_MS       1500u

/* Power ------------------------------------------------------------------ */
#define KF_IDLE_TIMEOUT_S 30u

#endif /* KF_CONFIG_H */
