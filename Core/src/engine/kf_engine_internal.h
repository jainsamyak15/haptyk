// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/**
 * @file kf_engine_internal.h
 * @brief Engine internals shared by kf_engine.c and the real-time units.
 */
#ifndef KF_ENGINE_INTERNAL_H
#define KF_ENGINE_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#include <kf/kf_config.h>
#include <kf/kf_engine.h>

#include "internal/kf_spsc.h"

KF_SPSC_DEFINE(kf_voice_queue, kf_voice_req, KF_VOICE_QUEUE_CAPACITY)

/** Queue owned by one producer thread; NULL for an invalid id. */
kf_voice_queue *kf_engine_voice_queue(kf_queue which);

void kf_engine_count_dropped(void);
void kf_engine_count_stale(void);
float kf_engine_volume(void);
bool kf_engine_muted(void);

/**
 * Renders one buffer into `channel_count` non-interleaved float channels.
 * Drains both queues first. Returns true when the output is silent, so the
 * CoreAudio callback can set kAudioUnitRenderAction_OutputIsSilence. RT-safe.
 */
bool kf_engine_render(float *const *channels, uint32_t channel_count, uint32_t frames,
                      uint64_t now_ns);

#endif /* KF_ENGINE_INTERNAL_H */
