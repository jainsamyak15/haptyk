// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#include <math.h>

#include <kf/kf_engine.h>

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
