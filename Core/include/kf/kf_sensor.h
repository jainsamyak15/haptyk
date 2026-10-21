// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/**
 * @file kf_sensor.h
 * @brief The MacBook motion sensor: lifecycle, state and key resolution.
 *
 * The sensor thread owns the sample ring and the calibration. Keys arrive from
 * the keytap thread through a lock-free pending queue; once a key's window has
 * closed the sensor thread measures its force and queues its voices.
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
    KF_SENSOR_STOPPED,    /**< Present, closed (idle). */
    KF_SENSOR_LIVE,       /**< Open and delivering fresh samples. */
    KF_SENSOR_STALE,      /**< Open, but no sample within KF_SENSOR_STALE_NS. */
    KF_SENSOR_DENIED,     /**< Present, but access was refused. */
} kf_sensor_state;

/** Cheap presence check. Main thread. */
bool kf_sensor_probe(void);

/** Opens the device and starts the sensor thread. Main thread. */
kf_status kf_sensor_start(void);

/** Stops the thread, then resolves every pending key at the default velocity. Main thread. */
void kf_sensor_stop(void);

/** Current state, with LIVE downgraded to STALE when samples stopped. Any thread; RT-safe. */
kf_sensor_state kf_sensor_state_now(uint64_t now_ns);

/** Hands a key-down to the sensor thread. Keytap thread only. False when the queue is full. */
bool kf_sensor_push_pending(const kf_key_event *event);

/** Replaces the calibration. Only while stopped; live hand-over arrives in M2. Main thread. */
kf_status kf_sensor_set_calibration(const kf_calibration *cal);

/** Velocity of the most recently resolved key (0..1), or -1 before the first. Any thread. */
float kf_sensor_last_velocity(void);

/** Diagnostics counters. */
typedef struct kf_sensor_counters {
    uint64_t samples;         /**< Samples processed. */
    uint64_t keys_resolved;   /**< Keys given a measured velocity. */
    uint64_t keys_defaulted;  /**< Keys given KF_DEFAULT_VELOCITY. */
    uint64_t dropped_reports; /**< Reports that failed to decode. */
} kf_sensor_counters;

/** Snapshot of the counters. Any thread. */
void kf_sensor_get_counters(kf_sensor_counters *out);

#ifdef __cplusplus
}
#endif

#endif /* KF_SENSOR_H */
