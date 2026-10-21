// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/* Engine lifecycle, shared state and counters (main thread). */

#include <kf/kf_engine.h>

#include <stdatomic.h>
#include <string.h>

#include "engine/kf_engine_internal.h"

static kf_voice_queue g_queues[KF_QUEUE_COUNT];
static _Atomic(float) g_volume = 1.0f;
static atomic_bool g_muted;
static _Atomic(uint64_t) g_dropped;
static _Atomic(uint64_t) g_stale;

kf_voice_queue *kf_engine_voice_queue(kf_queue which) {
    if ((int)which < 0 || (int)which >= (int)KF_QUEUE_COUNT) {
        return NULL;
    }
    return &g_queues[which];
}

void kf_engine_count_dropped(void) {
    atomic_fetch_add_explicit(&g_dropped, 1u, memory_order_relaxed);
}

void kf_engine_count_stale(void) {
    atomic_fetch_add_explicit(&g_stale, 1u, memory_order_relaxed);
}

float kf_engine_volume(void) {
    return atomic_load_explicit(&g_volume, memory_order_relaxed);
}

bool kf_engine_muted(void) {
    return atomic_load_explicit(&g_muted, memory_order_relaxed);
}

kf_status kf_engine_init(void) {
    for (int q = 0; q < (int)KF_QUEUE_COUNT; ++q) {
        kf_voice_queue_reset(&g_queues[q]);
    }
    atomic_store_explicit(&g_dropped, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_stale, 0u, memory_order_relaxed);
    return KF_OK;
}

kf_status kf_engine_start(void) {
    /*
     * TODO(M1): create kAudioUnitSubType_DefaultOutput (Float32, non-interleaved, stereo,
     * device rate); request KF_IO_BUFFER_FRAMES on this unit only; install the render
     * callback (kf_render_rt.c) which calls kf_engine_render(); listen for device and
     * sample-rate changes; record start latency and the granted buffer size.
     */
    return KF_ERR_NOT_IMPLEMENTED;
}

void kf_engine_stop(void) {
    /* TODO(M1): stop and dispose of the output unit; remove property listeners. */
}

bool kf_engine_is_running(void) {
    return false; /* TODO(M1) */
}

void kf_engine_set_volume(float volume) {
    float v = volume;
    if (!(v >= 0.0f)) {
        v = 0.0f; /* Also maps NaN to 0. */
    }
    if (v > 1.0f) {
        v = 1.0f;
    }
    atomic_store_explicit(&g_volume, v, memory_order_relaxed);
}

void kf_engine_set_muted(bool muted) {
    atomic_store_explicit(&g_muted, muted, memory_order_relaxed);
}

void kf_engine_set_pack(kf_pack *pack) {
    /*
     * TODO(M1): publish `pack` with an atomic exchange; the audio thread swaps it in at
     * the start of its next callback and bumps an epoch counter; free the previous pack
     * here only after the epoch changes (poll every 10 ms, at most 1 s).
     */
    kf_pack_free(pack);
}

void kf_engine_get_counters(kf_engine_counters *out) {
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof *out);
    out->requests_dropped = atomic_load_explicit(&g_dropped, memory_order_relaxed);
    out->requests_stale = atomic_load_explicit(&g_stale, memory_order_relaxed);
}
