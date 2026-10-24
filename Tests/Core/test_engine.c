// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#include <math.h>
#include <stdatomic.h>

#include <kf/kf_engine.h>
#include <kf/kf_pack.h>

#include "engine/kf_engine_internal.h"
#include "kf_test.h"

#define MS 1000000ULL

static kf_key_event key(bool is_down, uint64_t t_ns) {
    const kf_key_event event = {
        .t_ns = t_ns,
        .group = KF_GROUP_SPACE,
        .row = KF_ROW_SPACE,
        .pan = 50,
        .is_down = is_down,
        .builtin = true,
    };
    return event;
}

void test_emit_key_down_blends_tiers(void) {
    KF_EXPECT(kf_engine_init() == KF_OK);
    const kf_key_event down = key(true, 1000u);
    KF_EXPECT(kf_emit_keystroke(&down, 0.5f, KF_QUEUE_KEYTAP));

    kf_voice_queue *q = kf_engine_voice_queue(KF_QUEUE_KEYTAP);
    kf_voice_req a = {.t_ns = 0u};
    kf_voice_req b = {.t_ns = 0u};
    kf_voice_req extra = {.t_ns = 0u};
    KF_EXPECT(kf_voice_queue_pop(q, &a));
    KF_EXPECT(kf_voice_queue_pop(q, &b));
    KF_EXPECT(!kf_voice_queue_pop(q, &extra));
    KF_EXPECT(a.tier == KF_TIER_MEDIUM && b.tier == KF_TIER_HARD);
    KF_EXPECT_NEAR(a.gain, sqrt(0.5) * 0.85, 1e-5); /* Level 0.7 + 0.3 * 0.5. */
    KF_EXPECT_NEAR(b.gain, sqrt(0.5) * 0.85, 1e-5);
    KF_EXPECT_NEAR(a.pan, 0.5, 1e-6);
    KF_EXPECT(!a.is_up && a.group == KF_GROUP_SPACE && a.t_ns == 1000u);
    KF_EXPECT(kf_engine_voice_queue(KF_QUEUE_COUNT) == NULL);
}

void test_emit_key_up_plays_release(void) {
    KF_EXPECT(kf_engine_init() == KF_OK);
    const kf_key_event up = key(false, 2000u);
    KF_EXPECT(kf_emit_keystroke(&up, 1.0f, KF_QUEUE_SENSOR));
    kf_voice_req req = {.t_ns = 0u};
    KF_EXPECT(kf_voice_queue_pop(kf_engine_voice_queue(KF_QUEUE_SENSOR), &req));
    KF_EXPECT(req.is_up);
    KF_EXPECT_NEAR(req.gain, KF_RELEASE_GAIN, 1e-6);
    KF_EXPECT(!kf_voice_queue_pop(kf_engine_voice_queue(KF_QUEUE_SENSOR), &req));
}

void test_emit_drops_when_queue_full(void) {
    KF_EXPECT(kf_engine_init() == KF_OK);
    const kf_key_event down = key(true, 0u);
    for (uint32_t i = 0u; i < KF_VOICE_QUEUE_CAPACITY; ++i) {
        KF_EXPECT(kf_emit_keystroke(&down, 0.0f, KF_QUEUE_KEYTAP)); /* One voice each. */
    }
    KF_EXPECT(!kf_emit_keystroke(&down, 0.0f, KF_QUEUE_KEYTAP));
    kf_engine_counters counters;
    kf_engine_get_counters(&counters);
    KF_EXPECT(counters.requests_dropped == 1u);
}

static uint32_t count_boom(kf_queue which, uint32_t *voices) {
    kf_voice_req req = {.t_ns = 0u};
    uint32_t booms = 0u;
    *voices = 0u;
    while (kf_voice_queue_pop(kf_engine_voice_queue(which), &req)) {
        (*voices)++;
        if (req.group == KF_SOUND_BOOM) {
            booms++;
            KF_EXPECT(!req.is_up && req.tier == KF_TIER_SOFT);
            KF_EXPECT_NEAR(req.gain, KF_BIG_ENTER_GAIN, 1e-6);
            KF_EXPECT_NEAR(req.pan, 0.5, 1e-6); /* At the key's position. */
        }
    }
    return booms;
}

void test_emit_big_enter_adds_boom(void) {
    KF_EXPECT(kf_engine_init() == KF_OK);
    kf_key_event enter = key(true, 1000u);
    enter.group = KF_GROUP_ENTER;
    uint32_t voices = 0u;
    KF_EXPECT(kf_emit_keystroke(&enter, 1.0f, KF_QUEUE_SENSOR));
    KF_EXPECT(count_boom(KF_QUEUE_SENSOR, &voices) == 0u && voices == 1u); /* Off by default. */

    kf_engine_set_big_enter(true);
    KF_EXPECT(kf_emit_keystroke(&enter, 1.0f, KF_QUEUE_SENSOR));
    KF_EXPECT(count_boom(KF_QUEUE_SENSOR, &voices) == 1u && voices == 2u);
    KF_EXPECT(kf_emit_keystroke(&enter, KF_BIG_ENTER_VELOCITY, KF_QUEUE_SENSOR));
    KF_EXPECT(count_boom(KF_QUEUE_SENSOR, &voices) == 1u); /* At the threshold. */
    KF_EXPECT(kf_emit_keystroke(&enter, KF_BIG_ENTER_VELOCITY - 0.01f, KF_QUEUE_SENSOR));
    KF_EXPECT(count_boom(KF_QUEUE_SENSOR, &voices) == 0u);
    /* Unmeasured keys play at the default velocity, far below the threshold. */
    KF_EXPECT(kf_emit_keystroke(&enter, -1.0f, KF_QUEUE_KEYTAP));
    KF_EXPECT(count_boom(KF_QUEUE_KEYTAP, &voices) == 0u);
    /* Only Enter, and only its key-down. */
    const kf_key_event space = key(true, 1000u);
    KF_EXPECT(kf_emit_keystroke(&space, 1.0f, KF_QUEUE_SENSOR));
    KF_EXPECT(count_boom(KF_QUEUE_SENSOR, &voices) == 0u);
    enter.is_down = false;
    KF_EXPECT(kf_emit_keystroke(&enter, 1.0f, KF_QUEUE_KEYTAP));
    KF_EXPECT(count_boom(KF_QUEUE_KEYTAP, &voices) == 0u && voices == 1u);
    kf_engine_set_big_enter(false);
    KF_EXPECT(kf_engine_init() == KF_OK);
}

void test_render_drains_queues_and_is_silent(void) {
    KF_EXPECT(kf_engine_init() == KF_OK);
    const kf_key_event down = key(true, 100u * MS);
    KF_EXPECT(kf_emit_keystroke(&down, 0.5f, KF_QUEUE_KEYTAP));
    KF_EXPECT(kf_emit_keystroke(&down, 0.5f, KF_QUEUE_SENSOR));

    static float left[KF_IO_BUFFER_FRAMES];
    static float right[KF_IO_BUFFER_FRAMES];
    for (uint32_t i = 0u; i < KF_IO_BUFFER_FRAMES; ++i) {
        left[i] = 1.0f;
        right[i] = -1.0f;
    }
    float *const channels[2] = {left, right};
    KF_EXPECT(kf_engine_render(channels, 2u, KF_IO_BUFFER_FRAMES, 101u * MS));
    bool zeroed = true;
    for (uint32_t i = 0u; i < KF_IO_BUFFER_FRAMES; ++i) {
        zeroed = zeroed && left[i] == 0.0f && right[i] == 0.0f;
    }
    KF_EXPECT(zeroed);
    KF_EXPECT(kf_voice_queue_size_approx(kf_engine_voice_queue(KF_QUEUE_KEYTAP)) == 0u);
    KF_EXPECT(kf_voice_queue_size_approx(kf_engine_voice_queue(KF_QUEUE_SENSOR)) == 0u);
    kf_engine_counters counters;
    kf_engine_get_counters(&counters);
    KF_EXPECT(counters.requests_stale == 0u);
}

void test_render_drops_stale_requests(void) {
    KF_EXPECT(kf_engine_init() == KF_OK);
    const kf_key_event down = key(true, 100u * MS);
    KF_EXPECT(kf_emit_keystroke(&down, 0.5f, KF_QUEUE_KEYTAP)); /* Two voices. */
    static float mono[KF_IO_BUFFER_FRAMES];
    float *const channels[1] = {mono};
    (void)kf_engine_render(channels, 1u, KF_IO_BUFFER_FRAMES, 400u * MS);
    kf_engine_counters counters;
    kf_engine_get_counters(&counters);
    KF_EXPECT(counters.requests_stale == 2u);
}

/* ------------------------------------------------------------------ mixer */

#define RENDER_FRAMES 128u

static int16_t g_dc[48000]; /* A constant: makes gains readable straight off the output. */

static kf_pack *dc_pack(int16_t level, uint32_t frames) {
    for (uint32_t i = 0u; i < frames; ++i) {
        g_dc[i] = level;
    }
    const kf_pack_pcm sounds[] = {
        {.group = KF_GROUP_ALPHA,
         .is_up = 0u,
         .tier = KF_TIER_SOFT,
         .samples = g_dc,
         .frames = frames},
        {.group = KF_GROUP_ALPHA, .is_up = 1u, .tier = 0u, .samples = g_dc, .frames = frames},
    };
    return kf_pack_from_pcm(sounds, 2u, 48000.0, NULL, 0u);
}

static void fresh_engine(kf_pack *pack) {
    KF_EXPECT(kf_engine_init() == KF_OK);
    kf_engine_set_pack(pack); /* Stopped: swapped in at once. */
    KF_EXPECT(kf_engine_render_pack() == pack);
}

static kf_key_event key_at(int8_t pan, uint64_t t_ns) {
    kf_key_event event = key(true, t_ns);
    event.group = KF_GROUP_ALPHA;
    event.pan = pan;
    return event;
}

static bool render_frames(float *left, float *right, uint32_t frames, uint64_t now_ns) {
    float *const channels[2] = {left, right};
    return kf_engine_render(channels, 2u, frames, now_ns);
}

static bool render_stereo(float *left, float *right, uint64_t now_ns) {
    return render_frames(left, right, RENDER_FRAMES, now_ns);
}

void test_mixer_plays_with_constant_power_pan(void) {
    fresh_engine(dc_pack(8192, 4800u)); /* 0.25 full scale. */
    static float l[RENDER_FRAMES];
    static float r[RENDER_FRAMES];
    const int8_t pans[] = {-100, 0, 100};
    const double expect_l[] = {1.0, 0.70710678, 0.0};
    const double expect_r[] = {0.0, 0.70710678, 1.0};
    for (int i = 0; i < 3; ++i) {
        kf_engine_render_reset(48000.0);
        const kf_key_event down = key_at(pans[i], 10u * MS);
        KF_EXPECT(kf_emit_keystroke(&down, 0.0f, KF_QUEUE_KEYTAP)); /* v = 0: soft only, 0.7. */
        KF_EXPECT(!render_stereo(l, r, 10u * MS));
        KF_EXPECT_NEAR(l[64], 0.25 * 0.7 * expect_l[i], 1e-4);
        KF_EXPECT_NEAR(r[64], 0.25 * 0.7 * expect_r[i], 1e-4);
    }
    /* Stereo width 0: every key sits in the centre. */
    kf_engine_render_reset(48000.0);
    kf_engine_set_stereo_width(0.0f);
    const kf_key_event left_key = key_at(-100, 20u * MS);
    KF_EXPECT(kf_emit_keystroke(&left_key, 0.0f, KF_QUEUE_KEYTAP));
    KF_EXPECT(!render_stereo(l, r, 20u * MS));
    KF_EXPECT_NEAR(l[10], r[10], 1e-6);
    kf_engine_set_stereo_width(1.0f);
    kf_engine_set_pack(NULL);
}

void test_mixer_plays_the_boom(void) {
    for (uint32_t i = 0u; i < 4800u; ++i) {
        g_dc[i] = 8192; /* Keys: 0.25 full scale. */
    }
    static int16_t boom[4800];
    for (uint32_t i = 0u; i < 4800u; ++i) {
        boom[i] = 4096; /* Boom: 0.125. */
    }
    const kf_pack_pcm sounds[] = {
        {.group = KF_GROUP_ALPHA,
         .is_up = 0u,
         .tier = KF_TIER_SLAM,
         .samples = g_dc,
         .frames = 4800u},
        {.group = KF_SOUND_BOOM, .is_up = 0u, .tier = 0u, .samples = boom, .frames = 4800u},
    };
    fresh_engine(kf_pack_from_pcm(sounds, 2u, 48000.0, NULL, 0u));
    kf_engine_set_big_enter(true);
    kf_engine_set_volume(0.5f); /* Keeps the sum under the clipper's knee. */
    kf_engine_render_reset(48000.0);
    static float l[RENDER_FRAMES];
    static float r[RENDER_FRAMES];
    kf_key_event enter = key_at(0, 10u * MS);
    enter.group = KF_GROUP_ENTER; /* No Enter sounds: alpha's, then the boom on top. */
    KF_EXPECT(kf_emit_keystroke(&enter, 1.0f, KF_QUEUE_SENSOR));
    KF_EXPECT(!render_stereo(l, r, 10u * MS));
    KF_EXPECT(kf_engine_render_active_voices() == 2u);
    const double centre = 0.70710678 * 0.5;
    KF_EXPECT_NEAR(l[64], (0.25 * 1.0 + 0.125 * (double)KF_BIG_ENTER_GAIN) * centre, 1e-4);
    KF_EXPECT_NEAR(r[64], l[64], 1e-6);

    /* The same Enter with Big Enter off: the key alone. */
    kf_engine_set_big_enter(false);
    kf_engine_render_reset(48000.0);
    KF_EXPECT(kf_emit_keystroke(&enter, 1.0f, KF_QUEUE_SENSOR));
    KF_EXPECT(!render_stereo(l, r, 10u * MS));
    KF_EXPECT(kf_engine_render_active_voices() == 1u);
    KF_EXPECT_NEAR(l[64], 0.25 * centre, 1e-4);
    kf_engine_set_volume(1.0f);
    kf_engine_set_pack(NULL);
}

void test_mixer_voice_ends_and_goes_silent(void) {
    fresh_engine(dc_pack(8192, 200u)); /* Shorter than two buffers. */
    static float l[RENDER_FRAMES];
    static float r[RENDER_FRAMES];
    const kf_key_event down = key_at(0, 0u);
    KF_EXPECT(kf_emit_keystroke(&down, 0.0f, KF_QUEUE_KEYTAP));
    KF_EXPECT(!render_stereo(l, r, 1u));
    KF_EXPECT(kf_engine_render_active_voices() == 1u);
    KF_EXPECT(!render_stereo(l, r, 2u));
    KF_EXPECT(l[71] != 0.0f && l[72] == 0.0f); /* 200 - 128 = 72 frames left. */
    KF_EXPECT(kf_engine_render_active_voices() == 0u);
    KF_EXPECT(render_stereo(l, r, 3u)); /* Silent: the callback sets the silence flag. */
    kf_engine_set_pack(NULL);
}

void test_mixer_steals_the_oldest_voice(void) {
    fresh_engine(dc_pack(256, 48000u)); /* 1 s takes: nothing ends on its own. */
    static float l[RENDER_FRAMES];
    static float r[RENDER_FRAMES];
    for (uint32_t i = 0u; i < KF_MAX_VOICES; ++i) {
        const kf_key_event down = key_at(0, i);
        KF_EXPECT(kf_emit_keystroke(&down, 0.0f, KF_QUEUE_KEYTAP));
    }
    (void)render_stereo(l, r, 100u);
    KF_EXPECT(kf_engine_render_active_voices() == KF_MAX_VOICES);
    kf_engine_counters c;
    kf_engine_get_counters(&c);
    KF_EXPECT(c.voices_stolen == 0u);

    const kf_key_event extra = key_at(0, 200u);
    KF_EXPECT(kf_emit_keystroke(&extra, 0.0f, KF_QUEUE_KEYTAP));
    (void)render_frames(l, r, 64u, 300u); /* The stolen voice fades over 2 ms = 96 frames. */
    kf_engine_get_counters(&c);
    KF_EXPECT(c.voices_stolen == 1u);
    KF_EXPECT(kf_engine_render_active_voices() == KF_MAX_VOICES + 1u); /* One is fading. */
    (void)render_frames(l, r, 64u, 400u); /* Past 96 frames: the fade is over. */
    KF_EXPECT(kf_engine_render_active_voices() == KF_MAX_VOICES);

    /* A burst larger than the whole pool never fails and never exceeds it. */
    for (uint32_t i = 0u; i < 3u * (KF_MAX_VOICES + KF_VOICE_STEAL_RESERVE); ++i) {
        const kf_key_event down = key_at(0, 500u + i);
        KF_EXPECT(kf_emit_keystroke(&down, 0.0f, KF_QUEUE_KEYTAP));
    }
    (void)render_stereo(l, r, 600u);
    KF_EXPECT(kf_engine_render_active_voices() <= KF_MAX_VOICES + KF_VOICE_STEAL_RESERVE);
    kf_engine_set_pack(NULL);
}

void test_mixer_clipper_keeps_output_within_unity(void) {
    fresh_engine(dc_pack(32767, 4800u));
    static float l[RENDER_FRAMES];
    static float r[RENDER_FRAMES];
    for (uint32_t i = 0u; i < KF_MAX_VOICES; ++i) {
        const kf_key_event down = key_at((int8_t)((int)(i * 9u) - 100), 0u);
        KF_EXPECT(kf_emit_keystroke(&down, 1.0f, KF_QUEUE_SENSOR)); /* Slam, gain 1.0. */
    }
    KF_EXPECT(!render_stereo(l, r, 1u));
    float peak = 0.0f;
    for (uint32_t i = 0u; i < RENDER_FRAMES; ++i) {
        peak = fmaxf(peak, fmaxf(fabsf(l[i]), fabsf(r[i])));
    }
    KF_EXPECT(peak <= 1.0f);
    KF_EXPECT(peak > 0.95f); /* Loud, but soft-clipped instead of hard-clipped. */

    /* Below the threshold the clipper is transparent. */
    fresh_engine(dc_pack(3277, 4800u)); /* 0.1 */
    const kf_key_event one = key_at(-100, 0u);
    KF_EXPECT(kf_emit_keystroke(&one, 1.0f, KF_QUEUE_SENSOR));
    KF_EXPECT(!render_stereo(l, r, 1u));
    KF_EXPECT_NEAR(l[5], 3277.0 / 32768.0, 1e-5);
    kf_engine_set_pack(NULL);
}

void test_mixer_mute_and_volume(void) {
    fresh_engine(dc_pack(8192, 4800u));
    static float l[RENDER_FRAMES];
    static float r[RENDER_FRAMES];
    kf_engine_set_muted(true);
    kf_engine_render_reset(48000.0); /* Master already at 0, as after a long mute. */
    const kf_key_event down = key_at(0, 0u);
    KF_EXPECT(kf_emit_keystroke(&down, 0.0f, KF_QUEUE_KEYTAP));
    KF_EXPECT(render_stereo(l, r, 1u)); /* Muted: silent, and the request is discarded. */
    KF_EXPECT(kf_engine_render_active_voices() == 0u);
    kf_engine_set_muted(false);
    KF_EXPECT(render_stereo(l, r, 2u)); /* Nothing was kept to play late. */
    KF_EXPECT(kf_emit_keystroke(&down, 0.0f, KF_QUEUE_KEYTAP));
    KF_EXPECT(!render_stereo(l, r, 3u)); /* Unmuting is instant. */

    /* A volume change glides instead of stepping. */
    kf_engine_set_volume(0.0f);
    (void)render_stereo(l, r, 4u);
    KF_EXPECT(l[0] > l[127] && l[127] > 0.0f);
    kf_engine_set_volume(1.0f);
    kf_engine_set_pack(NULL);
}

void test_mixer_pack_swap_fades_then_releases(void) {
    kf_pack *first = dc_pack(8192, 48000u);
    fresh_engine(first);
    static float l[RENDER_FRAMES];
    static float r[RENDER_FRAMES];
    const kf_key_event down = key_at(0, 0u);
    KF_EXPECT(kf_emit_keystroke(&down, 0.0f, KF_QUEUE_KEYTAP));
    (void)render_stereo(l, r, 1u);
    KF_EXPECT(kf_engine_render_active_voices() == 1u);

    /* Publish as kf_engine_set_pack() does while running, then act as the audio thread. */
    kf_engine_shared *s = kf_engine_state();
    const uint64_t target = atomic_load(&s->swaps) + 1u;
    kf_pack *second = dc_pack(4096, 48000u);
    atomic_store(&s->pending, second);
    (void)render_stereo(l, r, 2u);
    KF_EXPECT(kf_engine_render_pack() == second);
    KF_EXPECT(atomic_load(&s->retirements) < target); /* The old voice is still fading. */
    for (int i = 0; i < 3; ++i) {
        (void)render_stereo(l, r, 3u); /* 5 ms = 240 frames. */
    }
    KF_EXPECT(atomic_load(&s->retirements) >= target);
    kf_pack *released = NULL;
    KF_EXPECT(kf_pack_queue_pop(&s->released, &released) && released == first);
    kf_pack_free(released);
    KF_EXPECT(kf_engine_render_active_voices() == 0u);
    kf_engine_set_pack(NULL);
    KF_EXPECT(kf_engine_render_pack() == NULL);
}

void test_engine_callback_counters(void) {
    KF_EXPECT(kf_engine_init() == KF_OK);
    kf_engine_render_reset(48000.0); /* 128 frames = 2.667 ms. */
    for (int i = 0; i < 98; ++i) {
        kf_engine_note_callback(1000u, 1000u + 5000u, 128u); /* 5 us */
    }
    kf_engine_note_callback(1000u, 1000u + 300000u, 128u);  /* 300 us */
    kf_engine_note_callback(1000u, 1000u + 3000000u, 128u); /* 3 ms: an overrun. */
    kf_engine_counters c;
    kf_engine_get_counters(&c);
    KF_EXPECT(c.callbacks == 100u);
    KF_EXPECT(c.callback_overruns == 1u);
    KF_EXPECT(c.max_callback_ns == 3000000u);
    KF_EXPECT(c.callback_p50_ns == 8000u); /* The 4..8 us bucket's upper bound. */
    KF_EXPECT(c.callback_p99_ns == 304000u);
}
