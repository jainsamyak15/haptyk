// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/**
 * @file kf_engine_internal.h
 * @brief Engine internals shared by kf_engine.c and the real-time units.
 *
 * Threads: producers (keytap, sensor) push voice requests; the audio thread owns the mixer;
 * the main thread owns the lifecycle. They share only kf_engine_shared, through SPSC
 * queues and atomics. While the output unit is stopped no render can run, and the main
 * thread may call the kf_engine_render_* functions marked "stopped only".
 */
#ifndef KF_ENGINE_INTERNAL_H
#define KF_ENGINE_INTERNAL_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#include <kf/kf_config.h>
#include <kf/kf_engine.h>

#include "internal/kf_spsc.h"

/* A typedef, so the macro's `const T *` means a const pointer, not a pointer to const. */
typedef kf_pack *kf_pack_ptr;

KF_SPSC_DEFINE(kf_voice_queue, kf_voice_req, KF_VOICE_QUEUE_CAPACITY)
KF_SPSC_DEFINE(kf_pack_queue, kf_pack_ptr, KF_RELEASE_QUEUE_CAPACITY)

/** Published in `pending` to unload: distinct from NULL, which means "nothing pending". */
extern char kf_engine_no_pack_tag;
#define KF_PACK_NONE ((kf_pack *)(void *)&kf_engine_no_pack_tag)

typedef struct kf_engine_shared {
    kf_voice_queue queues[KF_QUEUE_COUNT]; /* One producer each; the audio thread consumes. */
    kf_pack_queue released;                /* Audio thread -> main: retired packs to free. */
    _Atomic(kf_pack *) pending;            /* Main -> audio thread: the next pack. */
    _Atomic(uint64_t) swaps;               /* Packs taken from `pending`. */
    _Atomic(uint64_t) retirements;         /* Swaps whose previous pack is no longer played. */
    _Atomic(float) volume;
    _Atomic(float) stereo_width;
    atomic_bool muted;
    atomic_bool big_enter; /* Labs: hard Enter adds the pack's boom. */
    /* Counters: each has a single writer. */
    _Atomic(uint64_t) dropped; /* Producers: separate threads, so fetch_add. */
    _Atomic(uint64_t) stale;
    _Atomic(uint64_t) stolen;
    _Atomic(uint64_t) callbacks;
    _Atomic(uint64_t) overruns;
    _Atomic(uint64_t) max_callback_ns;
    _Atomic(uint64_t) start_ns;
    _Atomic(uint64_t) start_latency_ns;
    atomic_bool first_callback_seen;
    _Atomic(uint32_t) callback_hist[KF_CALLBACK_HIST_BUCKETS + 1u]; /* Last: overflow. */
} kf_engine_shared;

kf_engine_shared *kf_engine_state(void);

/** Queue owned by one producer thread; NULL for an invalid id. */
kf_voice_queue *kf_engine_voice_queue(kf_queue which);

void kf_engine_count_dropped(void);

/**
 * Renders one buffer into `channel_count` non-interleaved float channels (stereo; mono is
 * downmixed; extra channels get silence). Swaps in a published pack, drains both queues,
 * mixes, applies the smoothed volume and the soft clipper. Returns true when the output is
 * silent, so the callback can set kAudioUnitRenderAction_OutputIsSilence. RT-safe.
 */
bool kf_engine_render(float *const *channels, uint32_t channel_count, uint32_t frames,
                      uint64_t now_ns);

/** Records one callback's timing in the counters. Audio thread; RT-safe. */
void kf_engine_note_callback(uint64_t start_ns, uint64_t end_ns, uint32_t frames);

/** Clears every voice and sets rate-dependent constants. Stopped only. */
void kf_engine_render_reset(double sample_rate);

/** Silences every voice, then completes any pending swap at once. Stopped only. */
void kf_engine_render_settle(void);

/** Voices currently sounding, including ones fading out. Tests and diagnostics. */
uint32_t kf_engine_render_active_voices(void);

/** The pack the mixer plays. Stopped only, or from tests. */
const kf_pack *kf_engine_render_pack(void);

#if defined(__APPLE__)
#include <AudioToolbox/AudioToolbox.h>
/** The output unit's render callback (kf_output_rt.c). RT-safe. */
OSStatus kf_engine_output_callback(void *ctx, AudioUnitRenderActionFlags *flags,
                                   const AudioTimeStamp *timestamp, UInt32 bus, UInt32 frames,
                                   AudioBufferList *io);
#endif

#endif /* KF_ENGINE_INTERNAL_H */
