// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/* Real-time translation unit: only RT-safe code (checked by `make check-rt`). */

#include <kf/kf_config.h>
#include <kf/kf_engine.h>
#include <kf/kf_velocity.h>

#include "engine/kf_engine_internal.h"
#include "internal/kf_rt.h"

/* Overall level rises gently with force: 0.7 at v = 0, 1.0 at v = 1. */
#define KF_LEVEL_BASE  0.7f
#define KF_LEVEL_RANGE 0.3f

static KF_RT_SAFE bool push_or_count(kf_voice_queue *queue, const kf_voice_req *req) {
    if (kf_voice_queue_push(queue, req)) {
        return true;
    }
    kf_engine_count_dropped();
    return false;
}

KF_RT_SAFE bool kf_emit_keystroke(const kf_key_event *event, float velocity, kf_queue queue) {
    kf_voice_queue *q = kf_engine_voice_queue(queue);
    if (q == NULL || event == NULL) {
        return false;
    }
    kf_voice_req req = {
        .t_ns = event->t_ns,
        .group = event->group,
        .tier = (uint8_t)KF_TIER_SOFT,
        .is_up = !event->is_down,
        .gain = KF_RELEASE_GAIN,
        .pan = (float)event->pan / 100.0f,
    };
    if (!event->is_down) {
        return push_or_count(q, &req);
    }

    float v = velocity;
    if (!(v >= 0.0f)) {
        v = KF_DEFAULT_VELOCITY; /* Negative or NaN: unmeasured. */
    }
    if (v > 1.0f) {
        v = 1.0f;
    }
    const float level = KF_LEVEL_BASE + KF_LEVEL_RANGE * v;
    const kf_tier_mix mix = kf_tier_mix_for(v);
    bool all_queued = true;
    for (uint8_t i = 0u; i < mix.count; ++i) {
        req.tier = mix.tier[i];
        req.gain = mix.gain[i] * level;
        all_queued = push_or_count(q, &req) && all_queued;
    }
    if (event->group == (uint8_t)KF_GROUP_ENTER && v >= KF_BIG_ENTER_VELOCITY &&
        atomic_load_explicit(&kf_engine_state()->big_enter, memory_order_relaxed)) {
        req.group = (uint8_t)KF_SOUND_BOOM; /* Big Enter: on top of the switch sound. */
        req.tier = (uint8_t)KF_TIER_SOFT;
        req.gain = KF_BIG_ENTER_GAIN;
        all_queued = push_or_count(q, &req) && all_queued;
    }
    return all_queued;
}
