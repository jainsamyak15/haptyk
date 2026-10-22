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
#define KF_SAMPLE_RATE_HZ         48000u        /**< Preferred output rate. */
#define KF_IO_BUFFER_FRAMES       128u          /**< Requested buffer: 2.7 ms at 48 kHz. */
#define KF_MAX_VOICES             24u           /**< Voices that sound at once. */
#define KF_VOICE_STEAL_RESERVE    8u            /**< Extra slots where stolen voices fade out. */
#define KF_VOICE_STEAL_FADE_NS    2000000ULL    /**< Fade of a stolen voice: 2 ms. */
#define KF_PACK_SWAP_FADE_NS      5000000ULL    /**< Fade of the old pack's voices on a swap. */
#define KF_VOLUME_SMOOTH_NS       10000000ULL   /**< Volume changes glide over about 10 ms. */
#define KF_SOFT_CLIP_THRESHOLD    0.8f          /**< The master soft-clips above this level. */
#define KF_VOICE_QUEUE_CAPACITY   256u          /**< Per-producer queue; power of two. */
#define KF_RELEASE_QUEUE_CAPACITY 8u            /**< Retired packs awaiting free; power of two. */
#define KF_MIN_VOICE_GAIN         0.05f         /**< Quieter tier voices are skipped. */
#define KF_RELEASE_GAIN           0.6f          /**< Gain of key-up sounds. */
#define KF_QUEUE_MAX_AGE_NS       150000000ULL  /**< Drop requests older than 150 ms. */
#define KF_PACK_SWAP_POLL_NS      10000000ULL   /**< kf_engine_set_pack() polls every 10 ms... */
#define KF_PACK_SWAP_TIMEOUT_NS   1000000000ULL /**< ...for at most 1 s. */
#define KF_CALLBACK_HIST_BUCKETS  128u          /**< Callback-time histogram for diagnostics... */
#define KF_CALLBACK_HIST_STEP_NS  4000u         /**< ...in 4 us steps (0..512 us). */
#define KF_MAX_OUTPUT_CHANNELS    2u            /**< Stereo; extra device channels get silence. */

/* Keys ------------------------------------------------------------------- */
#define KF_BUILTIN_MATCH_NS 5000000ULL /**< Key event to HID keyboard report match: +-5 ms. */

/* Sensor ----------------------------------------------------------------- */
#define KF_SENSOR_NOMINAL_HZ      1000ULL     /**< TBD-M0: measured sample rate. */
#define KF_ACCEL_RING_SAMPLES     4096u       /**< ~4 s at 1 kHz; power of two. */
#define KF_PENDING_QUEUE_CAPACITY 64u         /**< Keys awaiting sensor data. */
#define KF_SENSOR_STALE_NS        20000000ULL /**< No sample for 20 ms = stale. */
#define KF_PEAK_QUEUE_CAPACITY    256u        /**< Measured peaks awaiting the calibration UI. */
#define KF_ROLLING_KEYS           3u          /**< Shout Mode's rolling force: last 3 keys. */

/* Sensor device (measured in M0 on Mac17,3; docs/SENSOR_NOTES.md, "Device"). The matching
 * omits the product ID, which may differ between models. */
#define KF_SENSOR_VENDOR_ID    0x05ACu
#define KF_SENSOR_USAGE_PAGE   0xFF00u
#define KF_SENSOR_USAGE        0x0003u
#define KF_SENSOR_TRANSPORT    "SPU"
#define KF_SENSOR_REPORT_BYTES 22u /**< From the report descriptor. */

/* Report layout: TBD-M0. Not measured, because the sensor sends nothing while idle (ADR-0011).
 * While KF_SENSOR_LAYOUT_MEASURED is 0 the values below are inert placeholders, the sensor is
 * reported absent and Keyfeel runs in fallback mode. Fill them from `analyze.py inspect`. */
#ifndef KF_SENSOR_LAYOUT_MEASURED /* `make analyze` also builds the sensor with it set. */
#define KF_SENSOR_LAYOUT_MEASURED 0
#endif
#define KF_SENSOR_SAMPLE_TYPE   0u          /**< TBD-M0: kf_sample_type. */
#define KF_SENSOR_SAMPLE_OFFSET 0u          /**< TBD-M0: byte offset of the x, y, z triple. */
#define KF_SENSOR_SCALE_G       1.0f        /**< TBD-M0: g per raw unit. */
#define KF_PENDING_MAX_AGE_NS   40000000ULL /**< M2 watchdog: default after 40 ms. */

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

/* Labs ------------------------------------------------------------------- */
#define KF_BIG_ENTER_VELOCITY      0.85f          /**< Enter this hard or harder adds the boom. */
#define KF_BIG_ENTER_GAIN          0.9f           /**< Level of the boom. */
#define KF_SHOUT_FORCE             0.8f           /**< Rolling force that types capitals. */
#define KF_SHOUT_MAX_TIMEOUTS      3u             /**< Tap timeouts that end Shout Mode... */
#define KF_SHOUT_TIMEOUT_WINDOW_NS 60000000000ULL /**< ...within one minute. */
#define KF_FORCE_METER_POLL_MS     50u            /**< Force Meter refresh, only while typing. */
#define KF_FORCE_METER_SHOW_MS     1000u          /**< Each keystroke's force shows for 1 s. */

/* Stats (app) -------------------------------------------------------------- */
#define KF_STATS_FORCE_BUCKETS  10u    /**< Force histogram: log-spaced, the same for everyone. */
#define KF_STATS_FORCE_MIN_G    0.005f /**< TBD-M0: lower edge of the first bucket. */
#define KF_STATS_FORCE_MAX_G    1.0f   /**< TBD-M0: upper edge of the last bucket. */
#define KF_STATS_FLUSH_S        300u   /**< Written at most every 5 minutes, and on idle/quit. */
#define KF_STATS_RETENTION_DAYS 90u

#endif /* KF_CONFIG_H */
