// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/**
 * @file kf_engine.h
 * @brief Low-latency audio output: voice queues, mixer and lifecycle.
 *
 * Producers call kf_emit_keystroke(); the CoreAudio render callback drains both
 * queues at the start of each buffer. Nothing on that path allocates or locks.
 */
#ifndef KF_ENGINE_H
#define KF_ENGINE_H

#include <stdbool.h>
#include <stdint.h>

#include <kf/kf_pack.h>
#include <kf/kf_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Resets queues and counters. Call before any producer thread starts. Main thread. */
kf_status kf_engine_init(void);

/** Starts the output unit; returns once it runs. Main thread. */
kf_status kf_engine_start(void);

/** Stops the output unit. Main thread. */
void kf_engine_stop(void);

/** Any thread. */
bool kf_engine_is_running(void);

/** 0..1, smoothed on the audio thread. Any thread. */
void kf_engine_set_volume(float volume);

/** Silences output without stopping the engine (e.g. microphone in use). Any thread. */
void kf_engine_set_muted(bool muted);

/** Hands a loaded pack to the engine, which owns and eventually frees it. Main thread. */
void kf_engine_set_pack(kf_pack *pack);

/**
 * Queues the voices for one key event. Key-ups play the release sound;
 * key-downs blend two tiers by `velocity` (0..1). Call only from the queue's
 * own producer thread. Returns false if any request was dropped. RT-safe.
 */
bool kf_emit_keystroke(const kf_key_event *event, float velocity, kf_queue queue);

/** Diagnostics counters. */
typedef struct kf_engine_counters {
    uint64_t requests_dropped;      /**< Queue full on emit. */
    uint64_t requests_stale;        /**< Older than KF_QUEUE_MAX_AGE_NS when drained. */
    uint64_t callbacks;             /**< Render callbacks run. */
    uint64_t callback_overruns;     /**< Callbacks slower than their buffer period. */
    uint64_t max_callback_ns;       /**< Slowest callback. */
    uint64_t start_latency_ns;      /**< kf_engine_start() to first callback. */
    uint32_t granted_buffer_frames; /**< Buffer size the device granted. */
} kf_engine_counters;

/** Snapshot of the counters. Any thread. */
void kf_engine_get_counters(kf_engine_counters *out);

#ifdef __cplusplus
}
#endif

#endif /* KF_ENGINE_H */
