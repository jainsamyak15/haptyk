// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/* Real-time translation unit: only RT-safe code (checked by `make check-rt`). */

#include <math.h>
#include <string.h>

#include <kf/kf_config.h>
#include <kf/kf_engine.h>
#include <kf/kf_sensor.h>
#include <kf/kf_time.h>
#include <kf/kf_velocity.h>

#include "internal/kf_rt.h"
#include "internal/kf_select.h"
#include "sensor/kf_sensor_internal.h"

/* Pseudo-keystrokes one window apart while measuring: windows tile, so no bump is missed. */
#define NOISE_PROBE_STEP_NS (KF_WINDOW_PRE_NS + KF_WINDOW_POST_NS)

static KF_RT_SAFE void bump(_Atomic(uint64_t) *counter) {
    atomic_fetch_add_explicit(counter, 1u, memory_order_relaxed);
}

KF_RT_SAFE bool kf_sensor_push_pending(const kf_key_event *event) {
    return kf_pending_queue_push(&kf_sensor_context()->pending, event);
}

static KF_RT_SAFE void note_velocity(kf_sensor_ctx *ctx, float velocity) {
    atomic_store_explicit(&ctx->last_velocity, velocity, memory_order_relaxed);
    if (ctx->recent_count < KF_ROLLING_KEYS) {
        ctx->recent[ctx->recent_count++] = velocity;
    } else {
        memmove(&ctx->recent[0], &ctx->recent[1], (KF_ROLLING_KEYS - 1u) * sizeof(float));
        ctx->recent[KF_ROLLING_KEYS - 1u] = velocity;
    }
    if (ctx->recent_count == KF_ROLLING_KEYS) {
        float sum = 0.0f;
        for (uint32_t i = 0u; i < KF_ROLLING_KEYS; ++i) {
            sum += ctx->recent[i];
        }
        atomic_store_explicit(&ctx->rolling_velocity, sum / (float)KF_ROLLING_KEYS,
                              memory_order_relaxed);
    }
}

static KF_RT_SAFE void resolve_key(kf_sensor_ctx *ctx, const kf_key_event *key, bool measure) {
    float velocity = -1.0f;
    if (measure && kf_calibration_is_valid(&ctx->calibration)) {
        const float peak = kf_velocity_peak_g(&ctx->ring, key->t_ns, key->row, &ctx->calibration);
        if (peak >= 0.0f) {
            velocity = kf_velocity_from_peak(peak, &ctx->calibration);
            (void)kf_autoadapt_observe(&ctx->autoadapt, peak, &ctx->calibration);
            const uint8_t row = key->row < KF_ROW_COUNT ? key->row : (uint8_t)KF_ROW_FUNCTION;
            const kf_sensor_peak measured = {key->t_ns, peak / ctx->calibration.row_gain[row],
                                             velocity, row};
            if (!kf_peak_queue_push(&ctx->peaks, &measured)) {
                bump(&ctx->peaks_dropped);
            }
        }
    }
    if (velocity < 0.0f) {
        velocity = KF_DEFAULT_VELOCITY;
        bump(&ctx->keys_defaulted);
    } else {
        bump(&ctx->keys_resolved);
    }
    note_velocity(ctx, velocity);
    (void)kf_emit_keystroke(key, velocity, KF_QUEUE_SENSOR);
}

/* While measuring the noise floor: the estimator at a pseudo-keystroke whose window has just
 * closed, so the floor is what the core itself would read with no key pressed. A high percentile
 * is retained instead of one maximum so one accidental bump cannot reject every soft tap. */
static KF_RT_SAFE void probe_noise(kf_sensor_ctx *ctx, uint64_t now_ns) {
    const uint64_t until = atomic_load_explicit(&ctx->noise_until_ns, memory_order_acquire);
    if (until == 0u) {
        return;
    }
    if (now_ns >= until) {
        const float floor = kf_percentile_inplace(
            ctx->noise_samples, (int32_t)ctx->noise_sample_count, KF_NOISE_PERCENTILE);
        atomic_store_explicit(&ctx->noise_floor_g, floor, memory_order_release);
        atomic_store_explicit(&ctx->noise_until_ns, 0u, memory_order_release);
        return;
    }
    if (now_ns < ctx->next_noise_probe_ns || now_ns < KF_WINDOW_POST_NS) {
        return;
    }
    ctx->next_noise_probe_ns = now_ns + NOISE_PROBE_STEP_NS;
    const float peak =
        kf_velocity_peak_g(&ctx->ring, now_ns - KF_WINDOW_POST_NS, KF_ROW_HOME, &ctx->calibration);
    if (peak >= 0.0f) {
        const float raw = peak / ctx->calibration.row_gain[KF_ROW_HOME];
        if (ctx->noise_sample_count < KF_NOISE_HISTORY) {
            ctx->noise_samples[ctx->noise_sample_count++] = raw;
        }
    }
}

KF_RT_SAFE void kf_sensor_apply_pending_calibration(void) {
    kf_sensor_ctx *ctx = kf_sensor_context();
    kf_calibration cal;
    bool changed = false;
    while (kf_calibration_queue_pop(&ctx->calibrations, &cal)) {
        changed = true; /* Only the newest counts. */
    }
    if (changed) {
        ctx->calibration = cal;
        kf_autoadapt_init(&ctx->autoadapt, &cal);
    }
}

KF_RT_SAFE void kf_sensor_process_sample(const kf_accel_sample *sample) {
    kf_sensor_ctx *ctx = kf_sensor_context();
    kf_accel_ring_push(&ctx->ring, sample);
    uint64_t expected = 0u; /* First sample of this run: anchors the rate measurement. */
    (void)atomic_compare_exchange_strong_explicit(&ctx->first_sample_ns, &expected, sample->t_ns,
                                                  memory_order_relaxed, memory_order_relaxed);
    atomic_store_explicit(&ctx->last_sample_ns, sample->t_ns, memory_order_release);
    bump(&ctx->samples);

    /* Keys arrive in time order, so stop at the first whose window is still open. */
    kf_key_event key;
    while (kf_pending_queue_peek(&ctx->pending, &key) &&
           key.t_ns + KF_WINDOW_POST_NS <= sample->t_ns) {
        resolve_key(ctx, &key, true);
        (void)kf_pending_queue_pop(&ctx->pending, &key);
    }
    probe_noise(ctx, sample->t_ns);
}

KF_RT_SAFE void kf_sensor_resolve_expired(uint64_t now_ns) {
    kf_sensor_ctx *ctx = kf_sensor_context();
    kf_key_event key;
    while (kf_pending_queue_peek(&ctx->pending, &key) && now_ns > key.t_ns &&
           now_ns - key.t_ns > KF_PENDING_MAX_AGE_NS) {
        resolve_key(ctx, &key, false);
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

KF_RT_SAFE bool kf_sensor_decode_with(const kf_sensor_layout *layout, const uint8_t *report,
                                      size_t length, uint64_t t_ns, kf_accel_sample *out) {
    static const uint32_t k_size[3] = {2u, 4u, 4u};
    if (layout == NULL || report == NULL || layout->type > KF_SAMPLE_FLOAT32 ||
        length != layout->report_bytes || layout->offset + 3u * k_size[layout->type] > length) {
        return false;
    }
    float axis[3];
    const uint8_t *p = report + layout->offset;
    for (int i = 0; i < 3; ++i, p += k_size[layout->type]) {
        /* memcpy: reports are byte arrays, unaligned; little-endian like the host. */
        if (layout->type == KF_SAMPLE_INT16) {
            int16_t v;
            memcpy(&v, p, sizeof v);
            axis[i] = (float)v;
        } else if (layout->type == KF_SAMPLE_INT32) {
            int32_t v;
            memcpy(&v, p, sizeof v);
            axis[i] = (float)v;
        } else {
            memcpy(&axis[i], p, sizeof axis[i]);
        }
        axis[i] *= layout->scale_g;
        if (!isfinite(axis[i])) {
            return false;
        }
    }
    out->t_ns = t_ns;
    out->x = axis[0];
    out->y = axis[1];
    out->z = axis[2];
    return true;
}

KF_RT_SAFE bool kf_sensor_decode_report(const uint8_t *report, size_t length, uint64_t t_ns,
                                        kf_accel_sample *out) {
#if KF_SENSOR_LAYOUT_MEASURED
    static const kf_sensor_layout k_measured = {KF_SENSOR_REPORT_BYTES, KF_SENSOR_SAMPLE_OFFSET,
                                                KF_SENSOR_SAMPLE_TYPE, KF_SENSOR_SCALE_G};
    return kf_sensor_decode_with(&k_measured, report, length, t_ns, out);
#else
    (void)report;
    (void)length;
    (void)t_ns;
    (void)out;
    return false; /* Layout is intentionally disabled until this Mac is measured. */
#endif
}

#if defined(__APPLE__)

#include <IOKit/hid/IOHIDDevice.h>

void kf_sensor_on_report(void *ref, IOReturn result, void *sender, IOHIDReportType type,
                         uint32_t report_id, uint8_t *report, CFIndex length, uint64_t timestamp);
void kf_sensor_on_timer(CFRunLoopTimerRef timer, void *info);

/* Sensor thread: one report -> one sample.  M0 measured the HID timestamp as mach ticks on the
 * core clock, so it keeps the 400 Hz sample cadence even when macOS batches callbacks. */
KF_RT_SAFE void kf_sensor_on_report(void *ref, IOReturn result, void *sender, IOHIDReportType type,
                                    uint32_t report_id, uint8_t *report, CFIndex length,
                                    uint64_t timestamp) {
    (void)ref;
    (void)sender;
    (void)type;
    (void)report_id;
    const uint64_t now = timestamp != 0u ? kf_time_ticks_to_ns(timestamp) : kf_time_now_ns();
    kf_sensor_ctx *ctx = kf_sensor_context();
    kf_sensor_apply_pending_calibration();
    kf_accel_sample sample;
    if (result != kIOReturnSuccess || length < 0 ||
        !kf_sensor_decode_report(report, (size_t)length, now, &sample)) {
        bump(&ctx->dropped_reports);
        return;
    }
    if (atomic_load_explicit(&ctx->state, memory_order_relaxed) != (int)KF_SENSOR_LIVE) {
        kf_sensor_set_state(KF_SENSOR_LIVE);
    }
    kf_sensor_process_sample(&sample);
}

/* Sensor thread, every KF_PENDING_MAX_AGE_NS / 2 while open: keys never wait on a sensor that
 * stopped sending. */
KF_RT_SAFE void kf_sensor_on_timer(CFRunLoopTimerRef timer, void *info) {
    (void)timer;
    (void)info;
    kf_sensor_resolve_expired(kf_time_now_ns());
}

#endif
