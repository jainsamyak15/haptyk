// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/**
 * @file kf_sensor_internal.h
 * @brief Sensor module internals. Not part of the public API.
 *
 * Ownership: the ring, calibration and auto-adapt state belong to the sensor
 * thread while it runs and to the main thread while it is stopped. The pending
 * queue has one producer (keytap thread) and one consumer (sensor thread, or the
 * main thread after the sensor thread has been joined).
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

typedef struct kf_sensor_ctx {
    kf_pending_queue pending; /* Cache-line aligned; first to avoid padding. */
    _Atomic(uint64_t) last_sample_ns;
    _Atomic(uint64_t) samples;
    _Atomic(uint64_t) keys_resolved;
    _Atomic(uint64_t) keys_defaulted;
    _Atomic(uint64_t) dropped_reports;
    kf_accel_ring ring;
    _Atomic(int) state; /* kf_sensor_state */
    _Atomic(float) last_velocity;
    kf_calibration calibration;
    kf_autoadapt autoadapt;
} kf_sensor_ctx;

/** The single sensor context. */
kf_sensor_ctx *kf_sensor_context(void);

/** Clears ring, queue and counters; installs the default calibration if none is valid. Stopped
 * only. */
void kf_sensor_reset(void);

/** Publishes a new state. */
void kf_sensor_set_state(kf_sensor_state state);

/** Stores one decoded sample and resolves every key whose window has closed. Sensor thread;
 * RT-safe. */
void kf_sensor_process_sample(const kf_accel_sample *sample);

/** Resolves every pending key at the default velocity. Only once the sensor thread has stopped. */
void kf_sensor_flush_pending(void);

/** Decodes one HID input report (layout from docs/SENSOR_NOTES.md). Sensor thread; RT-safe. */
bool kf_sensor_decode_report(const uint8_t *report, size_t length, uint64_t t_ns,
                             kf_accel_sample *out);

#endif /* KF_SENSOR_INTERNAL_H */
