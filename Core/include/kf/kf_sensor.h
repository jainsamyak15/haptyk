// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/**
 * @file kf_sensor.h
 * @brief The MacBook motion sensor: lifecycle, state and key resolution.
 *
 * The sensor thread owns the sample ring and the calibration. Keys arrive from the keytap
 * thread through a lock-free pending queue; once a key's window has closed the sensor thread
 * measures its force and queues its voices. The device is opened read-only, without seizing
 * it, and no driver property is changed (ADR-0011).
 *
 * Until the report layout is measured (KF_SENSOR_LAYOUT_MEASURED in kf_config.h) the probe
 * reports no sensor and every key plays at the default velocity.
 */
#ifndef KF_SENSOR_H
#define KF_SENSOR_H

#include <stdbool.h>
#include <stdint.h>

#include <kf/kf_types.h>
#include <kf/kf_velocity.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum kf_sensor_state {
    KF_SENSOR_ABSENT = 0, /**< No usable sensor on this Mac. */
    KF_SENSOR_STOPPED,    /**< Present, closed (idle), or open and waiting for a first sample. */
    KF_SENSOR_LIVE,       /**< Open and delivering fresh samples. */
    KF_SENSOR_STALE,      /**< Open, but no sample within KF_SENSOR_STALE_NS. */
    KF_SENSOR_DENIED,     /**< Present, but access was refused. */
} kf_sensor_state;

/** Cheap presence check: one IOKit lookup, nothing opened. Main thread. */
bool kf_sensor_probe(void);

/**
 * Opens the device and starts the sensor thread. KF_ERR_NOT_FOUND without a usable sensor,
 * KF_ERR_NO_PERMISSION when opening is refused. Main thread.
 */
kf_status kf_sensor_start(void);

/** Stops the thread, then resolves every pending key at the default velocity. Main thread. */
void kf_sensor_stop(void);

/** Current state, with LIVE downgraded to STALE when samples stopped. Any thread; RT-safe. */
kf_sensor_state kf_sensor_state_now(uint64_t now_ns);

/** Hands a key-down to the sensor thread. Keytap thread only. False when the queue is full. */
bool kf_sensor_push_pending(const kf_key_event *event);

/** Replaces the calibration, also while running (taken at the next sample). Main thread.
 * KF_ERR_INVALID_ARGUMENT for an invalid calibration, KF_ERR_LIMIT when changes pile up. */
kf_status kf_sensor_set_calibration(const kf_calibration *cal);

/** Velocity of the most recently resolved key (0..1), or -1 before the first. Any thread. */
float kf_sensor_last_velocity(void);

/** Mean velocity of the last KF_ROLLING_KEYS keys (Shout Mode), or -1 before them. Any thread. */
float kf_sensor_rolling_velocity(void);

/** One measured keystroke, for the calibration wizard and stats. */
typedef struct kf_sensor_peak {
    uint64_t t_ns;  /**< Key-down time. */
    float raw_g;    /**< Peak before row compensation. */
    float velocity; /**< 0..1 with the calibration in use. */
    uint8_t row;    /**< kf_key_row. */
} kf_sensor_peak;

/** Takes the oldest measured keystroke not yet read. Main thread. False when none. */
bool kf_sensor_pop_peak(kf_sensor_peak *out);

/** Starts measuring the noise floor: the largest pseudo-keystroke peak over `duration_ns`
 * with no key pressed (calibration step 1). Main thread. */
void kf_sensor_begin_noise_measurement(uint64_t duration_ns);

/** The measured noise floor in g, or -1 while measuring or never measured. Any thread. */
float kf_sensor_noise_floor_g(void);

/** Diagnostics counters. */
typedef struct kf_sensor_counters {
    uint64_t samples;         /**< Samples processed. */
    uint64_t keys_resolved;   /**< Keys given a measured velocity. */
    uint64_t keys_defaulted;  /**< Keys given KF_DEFAULT_VELOCITY. */
    uint64_t dropped_reports; /**< Reports that failed to decode. */
    uint64_t peaks_dropped;   /**< Measured keys the UI did not read in time. */
    double sample_rate_hz;    /**< Measured over this run; 0 before two samples. */
} kf_sensor_counters;

/** Snapshot of the counters. Any thread. */
void kf_sensor_get_counters(kf_sensor_counters *out);

#ifdef __cplusplus
}
#endif

#endif /* KF_SENSOR_H */
