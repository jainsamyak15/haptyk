// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/* Real-time translation unit: only RT-safe code (checked by `make check-rt`).
 * The output unit's render callback: timing, channel mapping and the silence flag. */

#include <string.h>

#include <kf/kf_config.h>
#include <kf/kf_time.h>

#include "engine/kf_engine_internal.h"
#include "internal/kf_rt.h"

#if defined(__APPLE__)

KF_RT_SAFE OSStatus kf_engine_output_callback(void *ctx, AudioUnitRenderActionFlags *flags,
                                              const AudioTimeStamp *timestamp, UInt32 bus,
                                              UInt32 frames, AudioBufferList *io) {
    (void)ctx;
    (void)timestamp;
    (void)bus;
    const uint64_t start = kf_time_now_ns();
    float *channels[KF_MAX_OUTPUT_CHANNELS];
    uint32_t count = io->mNumberBuffers; /* Non-interleaved: one buffer per channel. */
    if (count > KF_MAX_OUTPUT_CHANNELS) {
        for (uint32_t i = KF_MAX_OUTPUT_CHANNELS; i < count; ++i) {
            memset(io->mBuffers[i].mData, 0, io->mBuffers[i].mDataByteSize);
        }
        count = KF_MAX_OUTPUT_CHANNELS;
    }
    for (uint32_t i = 0u; i < count; ++i) {
        channels[i] = (float *)io->mBuffers[i].mData;
    }
    if (kf_engine_render(channels, count, frames, start)) {
        *flags |= (AudioUnitRenderActionFlags)kAudioUnitRenderAction_OutputIsSilence;
    }
    kf_engine_note_callback(start, kf_time_now_ns(), frames);
    return noErr;
}

#else

typedef int kf_output_rt_needs_core_audio; /* Host builds: nothing to render to. */

#endif
