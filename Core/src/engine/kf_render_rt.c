// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/* Real-time translation unit: only RT-safe code (checked by `make check-rt`).
 *
 * The mixer. Owned by the audio thread while the output unit runs, by the main thread while
 * it is stopped. Inner loops are per voice and per block, never per sample branches, so the
 * compiler vectorizes them (int16 -> float, multiply-accumulate into both channels). */

#include <math.h>
#include <string.h>

#include <kf/kf_config.h>
#include <kf/kf_pack.h>
#include <kf/kf_time.h>

#include "engine/kf_engine_internal.h"
#include "internal/kf_rng.h"
#include "internal/kf_rt.h"

#define VOICE_POOL  (KF_MAX_VOICES + KF_VOICE_STEAL_RESERVE)
#define BLOCK       256u /* Frames mixed at a time; bounds the mono scratch buffer. */
#define NO_TAKE     0xffffu
#define INV_INT16   (1.0f / 32768.0f)
#define QUARTER_PI  0.785398163397448f
#define INV_SQRT2   0.707106781186548f
#define SILENT_GAIN 1e-4f

typedef struct voice {
    const int16_t *samples; /* NULL: the slot is free. */
    const kf_pack *pack;
    uint64_t order; /* Start order; the smallest is the oldest. */
    uint32_t length;
    uint32_t position;
    uint32_t fade_left; /* Frames until silent; 0 when not fading. */
    float gain_left;    /* Includes the int16 scale, the velocity gain and the pan. */
    float gain_right;
    float ramp; /* Fade multiplier now, and its change per frame. */
    float ramp_step;
} voice;

typedef struct mixer {
    voice voices[VOICE_POOL];
    const kf_pack *pack;
    kf_pack *retiring; /* The previous pack while its voices fade out. */
    uint64_t retiring_swap;
    uint64_t next_order;
    uint32_t active;
    uint32_t steal_fade_frames;
    uint32_t swap_fade_frames;
    float master; /* Smoothed volume. */
    float smooth_frames;
    float ns_per_frame;
    kf_rng rng;
    uint16_t last_take[KF_SOUND_SLOTS][KF_TIER_COUNT + 1u]; /* Last column: release. */
} mixer;

static mixer g_mixer;

/* ------------------------------------------------------------------ voices */

static KF_RT_SAFE void free_voice(mixer *m, voice *v) {
    v->samples = NULL;
    v->pack = NULL;
    m->active--;
}

static KF_RT_SAFE void begin_fade(voice *v, uint32_t frames) {
    if (frames == 0u) {
        frames = 1u;
    }
    if (v->fade_left != 0u && v->fade_left <= frames) {
        return; /* Already fading at least as fast. */
    }
    v->fade_left = frames;
    v->ramp_step = v->ramp / (float)frames;
}

/* A free slot. With KF_MAX_VOICES sounding, the oldest starts a 2 ms fade in its own slot
 * and the new voice takes a reserve slot. Only if every slot is busy (more than the pool
 * started within one fade) is the oldest fading voice replaced outright. */
static KF_RT_SAFE voice *claim_voice(mixer *m, kf_engine_shared *s) {
    voice *free_slot = NULL;
    voice *oldest = NULL;
    voice *oldest_fading = NULL;
    uint32_t sounding = 0u;
    for (uint32_t i = 0u; i < VOICE_POOL; ++i) {
        voice *v = &m->voices[i];
        if (v->samples == NULL) {
            if (free_slot == NULL) {
                free_slot = v;
            }
        } else if (v->fade_left == 0u) {
            sounding++;
            if (oldest == NULL || v->order < oldest->order) {
                oldest = v;
            }
        } else if (oldest_fading == NULL || v->order < oldest_fading->order) {
            oldest_fading = v;
        }
    }
    if (sounding >= KF_MAX_VOICES && oldest != NULL) {
        begin_fade(oldest, m->steal_fade_frames);
        atomic_store_explicit(&s->stolen,
                              atomic_load_explicit(&s->stolen, memory_order_relaxed) + 1u,
                              memory_order_relaxed);
        if (oldest_fading == NULL || oldest->order < oldest_fading->order) {
            oldest_fading = oldest;
        }
    }
    if (free_slot != NULL) {
        m->active++;
        return free_slot;
    }
    return oldest_fading; /* Busy slot reused: active count unchanged. */
}

static KF_RT_SAFE void start_voice(mixer *m, kf_engine_shared *s, const kf_voice_req *req,
                                   float width) {
    const kf_pack_sound *takes = NULL;
    const uint32_t count = kf_pack_sounds(m->pack, req->group, req->is_up, req->tier, &takes);
    if (count == 0u) {
        return; /* No pack, or a pack without release sounds. */
    }
    uint16_t *last = &m->last_take[req->group][req->is_up ? KF_TIER_COUNT : req->tier];
    const uint32_t take =
        kf_rng_pick_avoiding(&m->rng, count, *last == NO_TAKE ? KF_RNG_NONE : (uint32_t)*last);
    *last = (uint16_t)take;

    voice *v = claim_voice(m, s);
    if (v == NULL) {
        return;
    }
    float pan = req->pan * width;
    pan = pan < -1.0f ? -1.0f : (pan > 1.0f ? 1.0f : pan);
    const float angle = (pan + 1.0f) * QUARTER_PI; /* Constant power: L = cos, R = sin. */
    const float gain = req->gain * INV_INT16;
    v->samples = takes[take].samples;
    v->length = takes[take].frames;
    v->position = 0u;
    v->pack = m->pack;
    v->order = m->next_order++;
    v->fade_left = 0u;
    v->ramp = 1.0f;
    v->ramp_step = 0.0f;
    v->gain_left = cosf(angle) * gain;
    v->gain_right = sinf(angle) * gain;
}

static KF_RT_SAFE void silence_voices(mixer *m) {
    for (uint32_t i = 0u; i < VOICE_POOL; ++i) {
        m->voices[i].samples = NULL;
        m->voices[i].pack = NULL;
    }
    m->active = 0u;
}

/* ------------------------------------------------------------------ mixing */

static KF_RT_SAFE void mix_voice(mixer *m, voice *v, float *restrict left, float *restrict right,
                                 uint32_t frames) {
    uint32_t n = v->length - v->position;
    if (n > frames) {
        n = frames;
    }
    const int16_t *restrict src = v->samples + v->position;
    if (v->fade_left == 0u) {
        const float gl = v->gain_left;
        const float gr = v->gain_right;
        for (uint32_t i = 0u; i < n; ++i) {
            const float x = (float)src[i];
            left[i] += x * gl;
            right[i] += x * gr;
        }
    } else {
        if (n > v->fade_left) {
            n = v->fade_left;
        }
        const float gl = v->gain_left * v->ramp;
        const float gr = v->gain_right * v->ramp;
        const float dl = v->gain_left * v->ramp_step;
        const float dr = v->gain_right * v->ramp_step;
        for (uint32_t i = 0u; i < n; ++i) {
            const float x = (float)src[i];
            const float k = (float)i;
            left[i] += x * (gl - dl * k);
            right[i] += x * (gr - dr * k);
        }
        v->ramp -= v->ramp_step * (float)n;
        v->fade_left -= n;
        if (v->fade_left == 0u) {
            free_voice(m, v);
            return;
        }
    }
    v->position += n;
    if (v->position >= v->length) {
        free_voice(m, v);
    }
}

/* Unity below the threshold, then a smooth knee that approaches 1.0 and never exceeds it.
 * Branch-free so the master loop vectorizes. */
static KF_RT_SAFE inline float soft_clip(float x) {
    const float knee = 1.0f - KF_SOFT_CLIP_THRESHOLD;
    const float a = fabsf(x);
    const float over = fmaxf(a - KF_SOFT_CLIP_THRESHOLD, 0.0f);
    return copysignf(fminf(a, KF_SOFT_CLIP_THRESHOLD) + knee * over / (knee + over), x);
}

static KF_RT_SAFE void master_stereo(float *restrict left, float *restrict right, uint32_t n,
                                     float g0, float g1) {
    const float dg = (g1 - g0) / (float)n;
    for (uint32_t i = 0u; i < n; ++i) {
        const float g = g0 + dg * (float)i;
        left[i] = soft_clip(left[i] * g);
        right[i] = soft_clip(right[i] * g);
    }
}

static KF_RT_SAFE void master_mono(float *restrict out, const float *restrict right, uint32_t n,
                                   float g0, float g1) {
    const float dg = (g1 - g0) / (float)n;
    for (uint32_t i = 0u; i < n; ++i) {
        const float g = (g0 + dg * (float)i) * INV_SQRT2;
        out[i] = soft_clip((out[i] + right[i]) * g);
    }
}

/* ------------------------------------------------------------------ queues and packs */

static KF_RT_SAFE void discard_queues(kf_engine_shared *s) {
    kf_voice_req req;
    for (int q = 0; q < (int)KF_QUEUE_COUNT; ++q) {
        while (kf_voice_queue_pop(&s->queues[q], &req)) {
        }
    }
}

static KF_RT_SAFE void drain_queues(mixer *m, kf_engine_shared *s, uint64_t now_ns) {
    const float width = atomic_load_explicit(&s->stereo_width, memory_order_relaxed);
    kf_voice_req req;
    for (int q = 0; q < (int)KF_QUEUE_COUNT; ++q) {
        while (kf_voice_queue_pop(&s->queues[q], &req)) {
            if (now_ns > req.t_ns && now_ns - req.t_ns > KF_QUEUE_MAX_AGE_NS) {
                /* Played late is worse than not played. */
                atomic_store_explicit(&s->stale,
                                      atomic_load_explicit(&s->stale, memory_order_relaxed) + 1u,
                                      memory_order_relaxed);
                continue;
            }
            start_voice(m, s, &req, width);
        }
    }
}

static KF_RT_SAFE void swap_pack_if_published(mixer *m, kf_engine_shared *s) {
    if (m->retiring != NULL) {
        return; /* One swap at a time: the previous pack is still fading out. */
    }
    kf_pack *next = atomic_exchange_explicit(&s->pending, NULL, memory_order_acq_rel);
    if (next == NULL) {
        return;
    }
    kf_pack *old = (kf_pack *)(uintptr_t)m->pack; /* Owned by the engine; never written. */
    m->pack = next == KF_PACK_NONE ? NULL : next;
    memset(m->last_take, 0xff, sizeof m->last_take);
    const uint64_t swap = atomic_fetch_add_explicit(&s->swaps, 1u, memory_order_acq_rel) + 1u;
    if (old == NULL) {
        atomic_store_explicit(&s->retirements, swap, memory_order_release);
        return;
    }
    for (uint32_t i = 0u; i < VOICE_POOL; ++i) {
        voice *v = &m->voices[i];
        if (v->samples != NULL && v->pack == old) {
            begin_fade(v, m->swap_fade_frames);
        }
    }
    m->retiring = old;
    m->retiring_swap = swap;
}

/* Hands the previous pack back once no voice plays it; the main thread frees it. */
static KF_RT_SAFE void release_retired_if_done(mixer *m, kf_engine_shared *s) {
    if (m->retiring == NULL) {
        return;
    }
    for (uint32_t i = 0u; i < VOICE_POOL; ++i) {
        if (m->voices[i].samples != NULL && m->voices[i].pack == m->retiring) {
            return;
        }
    }
    if (!kf_pack_queue_push(&s->released, &m->retiring)) {
        return; /* Main has not drained the queue yet: try again next callback. */
    }
    atomic_store_explicit(&s->retirements, m->retiring_swap, memory_order_release);
    m->retiring = NULL;
}

/* ------------------------------------------------------------------ render */

static KF_RT_SAFE float glide(const mixer *m, float from, float to, uint32_t frames) {
    if (fabsf(to - from) < 1e-6f) {
        return to;
    }
    return from + (to - from) * (1.0f - expf(-(float)frames / m->smooth_frames));
}

static KF_RT_SAFE void zero_channels(float *const *channels, uint32_t channel_count,
                                     uint32_t frames) {
    for (uint32_t c = 0u; c < channel_count; ++c) {
        memset(channels[c], 0, (size_t)frames * sizeof(float));
    }
}

KF_RT_SAFE bool kf_engine_render(float *const *channels, uint32_t channel_count, uint32_t frames,
                                 uint64_t now_ns) {
    mixer *m = &g_mixer;
    kf_engine_shared *s = kf_engine_state();
    swap_pack_if_published(m, s);

    const float target = atomic_load_explicit(&s->muted, memory_order_relaxed)
                             ? 0.0f
                             : atomic_load_explicit(&s->volume, memory_order_relaxed);
    if (target <= SILENT_GAIN && m->master <= SILENT_GAIN) {
        /* Muted or volume 0: nothing would be heard, so start nothing (unmuting is instant). */
        discard_queues(s);
        silence_voices(m);
        m->master = target;
        zero_channels(channels, channel_count, frames);
        release_retired_if_done(m, s);
        return true;
    }
    drain_queues(m, s, now_ns);
    if (m->active == 0u || channel_count == 0u) {
        m->master = target; /* Nothing sounds: no glide is needed. */
        zero_channels(channels, channel_count, frames);
        release_retired_if_done(m, s);
        return true;
    }

    const bool mono = channel_count == 1u;
    float scratch[BLOCK]; /* Mono only: the right channel before the downmix. */
    for (uint32_t offset = 0u; offset < frames; offset += BLOCK) {
        const uint32_t n = frames - offset < BLOCK ? frames - offset : BLOCK;
        float *left = channels[0] + offset;
        float *right = mono ? scratch : channels[1] + offset;
        memset(left, 0, (size_t)n * sizeof(float));
        memset(right, 0, (size_t)n * sizeof(float));
        for (uint32_t i = 0u; i < VOICE_POOL && m->active > 0u; ++i) {
            if (m->voices[i].samples != NULL) {
                mix_voice(m, &m->voices[i], left, right, n);
            }
        }
        const float g1 = glide(m, m->master, target, n);
        if (mono) {
            master_mono(left, right, n, m->master, g1);
        } else {
            master_stereo(left, right, n, m->master, g1);
        }
        m->master = g1;
    }
    for (uint32_t c = KF_MAX_OUTPUT_CHANNELS; c < channel_count; ++c) {
        memset(channels[c], 0, (size_t)frames * sizeof(float));
    }
    release_retired_if_done(m, s);
    return false;
}

KF_RT_SAFE void kf_engine_note_callback(uint64_t start_ns, uint64_t end_ns, uint32_t frames) {
    kf_engine_shared *s = kf_engine_state();
    const uint64_t elapsed = end_ns > start_ns ? end_ns - start_ns : 0u;
    atomic_store_explicit(&s->callbacks,
                          atomic_load_explicit(&s->callbacks, memory_order_relaxed) + 1u,
                          memory_order_relaxed);
    if (!atomic_load_explicit(&s->first_callback_seen, memory_order_relaxed)) {
        const uint64_t started = atomic_load_explicit(&s->start_ns, memory_order_relaxed);
        atomic_store_explicit(&s->start_latency_ns, start_ns > started ? start_ns - started : 0u,
                              memory_order_relaxed);
        atomic_store_explicit(&s->first_callback_seen, true, memory_order_release);
    }
    if ((float)elapsed > (float)frames * g_mixer.ns_per_frame) {
        atomic_store_explicit(&s->overruns,
                              atomic_load_explicit(&s->overruns, memory_order_relaxed) + 1u,
                              memory_order_relaxed);
    }
    if (elapsed > atomic_load_explicit(&s->max_callback_ns, memory_order_relaxed)) {
        atomic_store_explicit(&s->max_callback_ns, elapsed, memory_order_relaxed);
    }
    uint64_t bucket = elapsed / KF_CALLBACK_HIST_STEP_NS;
    if (bucket > KF_CALLBACK_HIST_BUCKETS) {
        bucket = KF_CALLBACK_HIST_BUCKETS;
    }
    atomic_store_explicit(&s->callback_hist[bucket],
                          atomic_load_explicit(&s->callback_hist[bucket], memory_order_relaxed) +
                              1u,
                          memory_order_relaxed);
}

/* ------------------------------------------------------------------ stopped only */

void kf_engine_render_reset(double sample_rate) {
    mixer *m = &g_mixer;
    silence_voices(m);
    const double rate = sample_rate > 0.0 ? sample_rate : (double)KF_SAMPLE_RATE_HZ;
    m->steal_fade_frames = (uint32_t)(rate * (double)KF_VOICE_STEAL_FADE_NS / 1e9 + 0.5);
    m->swap_fade_frames = (uint32_t)(rate * (double)KF_PACK_SWAP_FADE_NS / 1e9 + 0.5);
    m->smooth_frames = (float)(rate * (double)KF_VOLUME_SMOOTH_NS / 1e9);
    m->ns_per_frame = (float)(1e9 / rate);
    const kf_engine_shared *state = kf_engine_state();
    m->master = atomic_load_explicit(&state->muted, memory_order_relaxed)
                    ? 0.0f
                    : atomic_load_explicit(&state->volume, memory_order_relaxed);
    memset(m->last_take, 0xff, sizeof m->last_take);
    kf_rng_seed(&m->rng, (uint32_t)kf_time_now_ns());
}

void kf_engine_render_settle(void) {
    mixer *m = &g_mixer;
    kf_engine_shared *s = kf_engine_state();
    silence_voices(m);
    release_retired_if_done(m, s);
    swap_pack_if_published(m, s);
    release_retired_if_done(m, s); /* No voice plays the old pack: retire it now. */
}

uint32_t kf_engine_render_active_voices(void) {
    return g_mixer.active;
}

const kf_pack *kf_engine_render_pack(void) {
    return g_mixer.pack;
}
