// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/* Real-time translation unit: only RT-safe code (checked by `make check-rt`). */

#include <kf/kf_engine.h>
#include <kf/kf_sensor.h>
#include <kf/kf_velocity.h>

#include "internal/kf_rt.h"
#include "sensor/kf_sensor_internal.h"

KF_RT_SAFE bool kf_sensor_push_pending(const kf_key_event *event) {
    return kf_pending_queue_push(&kf_sensor_context()->pending, event);
}

static KF_RT_SAFE void resolve_key(kf_sensor_ctx *ctx, const kf_key_event *key, bool measure) {
    float velocity = -1.0f;
    if (measure && kf_calibration_is_valid(&ctx->calibration)) {
        const float peak = kf_velocity_peak_g(&ctx->ring, key->t_ns, key->row, &ctx->calibration);
        if (peak >= 0.0f) {
            velocity = kf_velocity_from_peak(peak, &ctx->calibration);
            (void)kf_autoadapt_observe(&ctx->autoadapt, peak, &ctx->calibration);
        }
    }
    if (velocity < 0.0f) {
        velocity = KF_DEFAULT_VELOCITY;
        atomic_fetch_add_explicit(&ctx->keys_defaulted, 1u, memory_order_relaxed);
    } else {
        atomic_fetch_add_explicit(&ctx->keys_resolved, 1u, memory_order_relaxed);
    }
    atomic_store_explicit(&ctx->last_velocity, velocity, memory_order_relaxed);
    (void)kf_emit_keystroke(key, velocity, KF_QUEUE_SENSOR);
}

KF_RT_SAFE void kf_sensor_process_sample(const kf_accel_sample *sample) {
    kf_sensor_ctx *ctx = kf_sensor_context();
    kf_accel_ring_push(&ctx->ring, sample);
    atomic_store_explicit(&ctx->last_sample_ns, sample->t_ns, memory_order_release);
    atomic_fetch_add_explicit(&ctx->samples, 1u, memory_order_relaxed);

    /* Keys arrive in time order, so stop at the first whose window is still open. */
    kf_key_event key;
    while (kf_pending_queue_peek(&ctx->pending, &key) &&
           key.t_ns + KF_WINDOW_POST_NS <= sample->t_ns) {
        resolve_key(ctx, &key, true);
        (void)kf_pending_queue_pop(&ctx->pending, &key);
    }
}

void kf_sensor_flush_pending(void) {
    kf_sensor_ctx *ctx = kf_sensor_context();
    kf_key_event key;
    while (kf_pending_queue_pop(&ctx->pending, &key)) {
        resolve_key(ctx, &key, false);
    }
}

KF_RT_SAFE bool kf_sensor_decode_report(const uint8_t *report, size_t length, uint64_t t_ns,
                                        kf_accel_sample *out) {
    (void)report;
    (void)length;
    (void)t_ns;
    (void)out;
    /*
     * TODO(M0): decode per docs/SENSOR_NOTES.md (byte layout, axis order, scale to g).
     * Unit-test against recorded reports in Tests/Fixtures/sensor/. Count failures in
     * kf_sensor_ctx.dropped_reports.
     */
    return false;
}
