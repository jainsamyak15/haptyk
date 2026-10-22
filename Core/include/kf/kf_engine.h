// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/**
 * @file kf_engine.h
 * @brief Low-latency audio output: voice queues, mixer and lifecycle.
 *
 * Producers call kf_emit_keystroke(); the CoreAudio render callback drains both queues at
 * the start of each buffer, so a new voice starts within one buffer (2.7 ms at 128 frames).
 * Nothing on that path allocates, locks, logs or calls Objective-C.
 *
 * The output unit is kAudioUnitSubType_DefaultOutput. It asks for KF_IO_BUFFER_FRAMES on
 * its own unit only: the request is per process, so other apps and the device's settings are
 * untouched, and nothing persists after the engine stops.
 */
#ifndef KF_ENGINE_H
#define KF_ENGINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <kf/kf_pack.h>
#include <kf/kf_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Resets queues, voices and counters. Call once before any producer thread starts. */
kf_status kf_engine_init(void);

/**
 * Starts the output unit at the default device's nominal rate; returns once it runs.
 * Main thread. KF_ERR_AUDIO when CoreAudio refuses.
 */
kf_status kf_engine_start(void);

/** Stops and disposes of the output unit; silences every voice. Main thread. */
void kf_engine_stop(void);

/** Any thread. */
bool kf_engine_is_running(void);

/** Output rate while running, else 0. Any thread. */
double kf_engine_sample_rate(void);

/** Nominal rate of the current default output device, for loading packs. Main thread. */
kf_status kf_engine_default_output_rate(double *rate);

/** Name of the device in use (or the default output device), for diagnostics. Main thread. */
void kf_engine_output_device_name(char *buf, size_t len);

/** 0..1, smoothed on the audio thread. Any thread. */
void kf_engine_set_volume(float volume);

/** Silences output without stopping the engine (e.g. microphone in use). Any thread. */
void kf_engine_set_muted(bool muted);

/** Stereo spread of the key positions: 0 mono .. 1 full. Applies to new voices. Any thread. */
void kf_engine_set_stereo_width(float width);

/** Big Enter (Labs): a key-down on Enter at KF_BIG_ENTER_VELOCITY or harder also plays the
 * pack's boom (KF_SOUND_BOOM), if it holds one. Off by default. Any thread. */
void kf_engine_set_big_enter(bool enabled);

/**
 * Hands a pack to the engine, which owns it from now on; NULL unloads. While running, the
 * audio thread swaps it in at its next callback and fades the old pack's voices over
 * KF_PACK_SWAP_FADE_NS; this call waits (polling every 10 ms, at most 1 s) until the old pack
 * is no longer played, then frees it. Main thread.
 */
void kf_engine_set_pack(kf_pack *pack);

/** Called on the main queue when the default output device or its sample rate changes. */
typedef void (*kf_engine_change_cb)(void *ctx);

/** Installs the change handler (NULL removes it). The app restarts the engine and reloads
 * the pack at the new rate. Main thread. */
void kf_engine_set_change_handler(kf_engine_change_cb callback, void *ctx);

/**
 * Queues the voices for one key event. Key-ups play the release sound; key-downs blend two
 * tiers by `velocity` (0..1, negative for unmeasured), plus the boom for a Big Enter. Only a
 * measured velocity can reach the Big Enter threshold, so in practice the boom comes from the
 * sensor thread's queue (and from the main thread's previews). Call only from the queue's own
 * producer thread. Returns false if any request was dropped. RT-safe.
 */
bool kf_emit_keystroke(const kf_key_event *event, float velocity, kf_queue queue);

/** Diagnostics counters. */
typedef struct kf_engine_counters {
    uint64_t requests_dropped;  /**< Queue full on emit. */
    uint64_t requests_stale;    /**< Older than KF_QUEUE_MAX_AGE_NS when drained. */
    uint64_t voices_stolen;     /**< Oldest voice faded out to make room. */
    uint64_t callbacks;         /**< Render callbacks since start. */
    uint64_t callback_overruns; /**< Callbacks slower than their buffer period. */
    uint64_t max_callback_ns;   /**< Slowest callback. */
    uint64_t callback_p50_ns;   /**< Callback time percentiles, 4 us resolution. */
    uint64_t callback_p99_ns;
    uint64_t start_latency_ns;      /**< kf_engine_start() to the first callback. */
    uint32_t granted_buffer_frames; /**< Buffer size the device granted this process. */
    double sample_rate;             /**< Output rate while running. */
} kf_engine_counters;

/** Snapshot of the counters. Any thread. */
void kf_engine_get_counters(kf_engine_counters *out);

#ifdef __cplusplus
}
#endif

#endif /* KF_ENGINE_H */
