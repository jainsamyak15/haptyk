// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/**
 * @file kf_sensor_internal.h
 * @brief Sensor module internals. Not part of the public API.
 *
 * Ownership: the ring, calibration and auto-adapt state belong to the sensor thread while it
 * runs and to the main thread while it is stopped. Queues: pending keys (keytap -> sensor),
 * measured peaks (sensor -> main), calibration changes (main -> sensor), each with exactly
 * one producer and one consumer.
 */
#ifndef KF_SENSOR_INTERNAL_H
#define KF_SENSOR_INTERNAL_H

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

#include <kf/kf_accel.h>
#include <kf/kf_sensor.h>
#include <kf/kf_velocity.h>

#include "internal/kf_spsc.h"

KF_SPSC_DEFINE(kf_pending_queue, kf_key_event, KF_PENDING_QUEUE_CAPACITY)
KF_SPSC_DEFINE(kf_peak_queue, kf_sensor_peak, KF_PEAK_QUEUE_CAPACITY)
KF_SPSC_DEFINE(kf_calibration_queue, kf_calibration, 4u)

/** How the three axes sit in a report. The measured one is built from kf_config.h. */
typedef enum kf_sample_type {
    KF_SAMPLE_INT16 = 0,
    KF_SAMPLE_INT32 = 1,
    KF_SAMPLE_FLOAT32 = 2,
} kf_sample_type;

typedef struct kf_sensor_layout {
    uint32_t report_bytes; /**< Reports of any other length are rejected. */
    uint32_t offset;       /**< First byte of the x, y, z triple. */
    uint8_t type;          /**< kf_sample_type, little-endian. */
    float scale_g;         /**< g per raw unit. */
} kf_sensor_layout;

typedef struct kf_sensor_ctx {
    kf_pending_queue pending; /* Cache-line aligned; first to avoid padding. */
    kf_peak_queue peaks;
    kf_calibration_queue calibrations;
    _Atomic(uint64_t) first_sample_ns;
    _Atomic(uint64_t) last_sample_ns;
    _Atomic(uint64_t) samples;
    _Atomic(uint64_t) keys_resolved;
    _Atomic(uint64_t) keys_defaulted;
    _Atomic(uint64_t) dropped_reports;
    _Atomic(uint64_t) peaks_dropped;
    _Atomic(uint64_t) noise_until_ns; /* Measuring the noise floor until this time. */
    _Atomic(float) noise_floor_g;
    kf_accel_ring ring;
    _Atomic(int) state; /* kf_sensor_state */
    _Atomic(float) last_velocity;
    _Atomic(float) rolling_velocity;
    float recent[KF_ROLLING_KEYS];
    uint32_t recent_count;
    float noise_samples[KF_NOISE_HISTORY]; /* Sensor thread, while measuring. */
    uint32_t noise_sample_count;
    uint64_t next_noise_probe_ns;
    kf_calibration calibration;
    kf_autoadapt autoadapt;
} kf_sensor_ctx;

/** The single sensor context. */
kf_sensor_ctx *kf_sensor_context(void);

/** Clears rings, queues and counters; installs the default calibration if none is valid.
 * Stopped only. */
void kf_sensor_reset(void);

/** Publishes a new state. */
void kf_sensor_set_state(kf_sensor_state state);

/** Stores one decoded sample and resolves every key whose window has closed. Sensor thread;
 * RT-safe. */
void kf_sensor_process_sample(const kf_accel_sample *sample);

/** Resolves, at the default velocity, every pending key older than KF_PENDING_MAX_AGE_NS: keys
 * still play when samples stop arriving. Sensor thread (its timer); RT-safe. */
void kf_sensor_resolve_expired(uint64_t now_ns);

/** Takes a calibration handed over by kf_sensor_set_calibration(). Sensor thread; RT-safe. */
void kf_sensor_apply_pending_calibration(void);

/** Resolves every pending key at the default velocity. Only once the sensor thread has stopped. */
void kf_sensor_flush_pending(void);

/** Decodes one report with `layout`. False for a wrong length or non-finite values. RT-safe. */
bool kf_sensor_decode_with(const kf_sensor_layout *layout, const uint8_t *report, size_t length,
                           uint64_t t_ns, kf_accel_sample *out);

/** Decodes one HID input report with the measured layout (docs/SENSOR_NOTES.md). False while
 * the layout is unmeasured. Sensor thread; RT-safe. */
bool kf_sensor_decode_report(const uint8_t *report, size_t length, uint64_t t_ns,
                             kf_accel_sample *out);

/** One driver setting to write. */
typedef struct kf_driver_write {
    const char *key;
    int32_t value;
} kf_driver_write;

#define KF_DRIVER_MAX_WRITES 3u

/** The writes that switch reporting on at `level`, in order: reporting state, power state,
 * interval. Returns how many; 0 for NONE or an invalid level or interval. */
uint32_t kf_sensor_wake_plan(kf_sensor_wake level, uint32_t interval_us,
                             kf_driver_write out[KF_DRIVER_MAX_WRITES]);

/** The writes that put back the first `count` writes of `level`'s plan: each setting returns
 * to its value in `saved`, or to 0 where the driver showed none. Same order as the plan. */
uint32_t kf_sensor_restore_plan(kf_sensor_wake level, uint32_t count,
                                const kf_sensor_driver_state *saved,
                                kf_driver_write out[KF_DRIVER_MAX_WRITES]);

#endif /* KF_SENSOR_INTERNAL_H */
