// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/* Real-time translation unit: only RT-safe code (checked by `make check-rt`). */

#include <string.h>

#include "engine/kf_engine_internal.h"
#include "internal/kf_rt.h"

KF_RT_SAFE bool kf_engine_render(float *const *channels, uint32_t channel_count, uint32_t frames,
                                 uint64_t now_ns) {
    for (int q = 0; q < (int)KF_QUEUE_COUNT; ++q) {
        kf_voice_queue *queue = kf_engine_voice_queue((kf_queue)q);
        kf_voice_req req;
        while (kf_voice_queue_pop(queue, &req)) {
            if (now_ns > req.t_ns && now_ns - req.t_ns > KF_QUEUE_MAX_AGE_NS) {
                kf_engine_count_stale(); /* Played late is worse than not played. */
                continue;
            }
            /*
             * TODO(M1): start a voice from the active pack: round-robin take via
             * kf_rng_pick_avoiding(); constant-power pan scaled by stereo width; when
             * the pool is full, steal the oldest with a KF_VOICE_STEAL_FADE_FRAMES fade.
             */
        }
    }

    /*
     * TODO(M1): mix active voices (int16 -> float, vectorised); apply smoothed
     * kf_engine_volume() and kf_engine_muted(); soft-clip above 0.8. Return false
     * whenever any voice contributed to the buffer.
     */
    for (uint32_t c = 0u; c < channel_count; ++c) {
        memset(channels[c], 0, (size_t)frames * sizeof(float));
    }
    return true;
}
