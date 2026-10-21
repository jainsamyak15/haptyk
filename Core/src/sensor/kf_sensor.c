// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/* Sensor lifecycle and state (main thread). The hot path lives in kf_sensor_rt.c. */

#include <kf/kf_sensor.h>

#include <string.h>

#include "internal/kf_rt.h"
#include "sensor/kf_sensor_internal.h"

static kf_sensor_ctx g_ctx;

kf_sensor_ctx *kf_sensor_context(void) {
    return &g_ctx;
}

static kf_sensor_state load_state(void) {
    return (kf_sensor_state)atomic_load_explicit(&g_ctx.state, memory_order_acquire);
}

static bool is_running(kf_sensor_state state) {
    return state == KF_SENSOR_LIVE || state == KF_SENSOR_STALE;
}

void kf_sensor_set_state(kf_sensor_state state) {
    atomic_store_explicit(&g_ctx.state, (int)state, memory_order_release);
}

void kf_sensor_reset(void) {
    if (!kf_calibration_is_valid(&g_ctx.calibration)) {
        g_ctx.calibration = kf_calibration_default();
    }
    kf_accel_ring_reset(&g_ctx.ring);
    kf_pending_queue_reset(&g_ctx.pending);
    kf_autoadapt_init(&g_ctx.autoadapt, &g_ctx.calibration);
    atomic_store_explicit(&g_ctx.last_sample_ns, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_ctx.last_velocity, -1.0f, memory_order_relaxed);
    atomic_store_explicit(&g_ctx.samples, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_ctx.keys_resolved, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_ctx.keys_defaulted, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_ctx.dropped_reports, 0u, memory_order_relaxed);
    kf_sensor_set_state(KF_SENSOR_STOPPED);
}

bool kf_sensor_probe(void) {
    /* TODO(M0): match the device recorded in docs/SENSOR_NOTES.md with IOHIDManager. */
    return false;
}

kf_status kf_sensor_start(void) {
    /*
     * TODO(M2): open the HID device found in M0; start a pthread with a CFRunLoop at
     * QOS_CLASS_USER_INTERACTIVE; register the input-report callback, which calls
     * kf_sensor_decode_report() then kf_sensor_process_sample(); set KF_SENSOR_LIVE
     * on the first decoded sample.
     */
    return KF_ERR_NOT_IMPLEMENTED;
}

void kf_sensor_stop(void) {
    /*
     * TODO(M2): stop and join the sensor thread first. After the join, this thread is
     * the only consumer of the pending queue and the only producer of queue B.
     */
    kf_sensor_flush_pending();
    if (is_running(load_state())) {
        kf_sensor_set_state(KF_SENSOR_STOPPED);
    }
}

KF_RT_SAFE kf_sensor_state kf_sensor_state_now(uint64_t now_ns) {
    const kf_sensor_state state = load_state();
    if (state != KF_SENSOR_LIVE) {
        return state;
    }
    const uint64_t last = atomic_load_explicit(&g_ctx.last_sample_ns, memory_order_acquire);
    if (now_ns > last && now_ns - last > KF_SENSOR_STALE_NS) {
        return KF_SENSOR_STALE;
    }
    return KF_SENSOR_LIVE;
}

kf_status kf_sensor_set_calibration(const kf_calibration *cal) {
    if (!kf_calibration_is_valid(cal)) {
        return KF_ERR_INVALID_ARGUMENT;
    }
    if (is_running(load_state())) {
        return KF_ERR_STATE; /* TODO(M2): live hand-over through an atomic mailbox. */
    }
    g_ctx.calibration = *cal;
    kf_autoadapt_init(&g_ctx.autoadapt, cal);
    return KF_OK;
}

float kf_sensor_last_velocity(void) {
    return atomic_load_explicit(&g_ctx.last_velocity, memory_order_relaxed);
}

void kf_sensor_get_counters(kf_sensor_counters *out) {
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof *out);
    out->samples = atomic_load_explicit(&g_ctx.samples, memory_order_relaxed);
    out->keys_resolved = atomic_load_explicit(&g_ctx.keys_resolved, memory_order_relaxed);
    out->keys_defaulted = atomic_load_explicit(&g_ctx.keys_defaulted, memory_order_relaxed);
    out->dropped_reports = atomic_load_explicit(&g_ctx.dropped_reports, memory_order_relaxed);
}
