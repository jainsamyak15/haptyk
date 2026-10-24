// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/* Engine lifecycle, the pack handoff and counters (main thread). The audio thread's code is
 * in kf_render_rt.c and kf_output_rt.c. */

#include <kf/kf_engine.h>

#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <kf/kf_time.h>

#include "engine/kf_engine_internal.h"

#if defined(__APPLE__)
#include <CoreAudio/CoreAudio.h>
#include <dispatch/dispatch.h>
#endif

char kf_engine_no_pack_tag;

static kf_engine_shared g_state;
static atomic_bool g_running;
static _Atomic(double) g_rate;
static uint32_t g_granted_frames;
static kf_engine_change_cb g_change_cb;
static void *g_change_ctx;

kf_engine_shared *kf_engine_state(void) {
    return &g_state;
}

kf_voice_queue *kf_engine_voice_queue(kf_queue which) {
    if ((int)which < 0 || (int)which >= (int)KF_QUEUE_COUNT) {
        return NULL;
    }
    return &g_state.queues[which];
}

void kf_engine_count_dropped(void) {
    atomic_fetch_add_explicit(&g_state.dropped, 1u, memory_order_relaxed);
}

static void reset_counters(void) {
    atomic_store_explicit(&g_state.callbacks, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_state.overruns, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_state.max_callback_ns, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_state.start_latency_ns, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_state.first_callback_seen, false, memory_order_relaxed);
    for (uint32_t i = 0u; i <= KF_CALLBACK_HIST_BUCKETS; ++i) {
        atomic_store_explicit(&g_state.callback_hist[i], 0u, memory_order_relaxed);
    }
}

kf_status kf_engine_init(void) {
    for (int q = 0; q < (int)KF_QUEUE_COUNT; ++q) {
        kf_voice_queue_reset(&g_state.queues[q]);
    }
    atomic_store_explicit(&g_state.dropped, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_state.stale, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_state.stolen, 0u, memory_order_relaxed);
    reset_counters();
    atomic_store_explicit(&g_state.volume, 1.0f, memory_order_relaxed);
    atomic_store_explicit(&g_state.stereo_width, 1.0f, memory_order_relaxed);
    atomic_store_explicit(&g_state.muted, false, memory_order_relaxed);
    atomic_store_explicit(&g_state.big_enter, false, memory_order_relaxed);
    kf_engine_render_reset((double)KF_SAMPLE_RATE_HZ);
    return KF_OK;
}

bool kf_engine_is_running(void) {
    return atomic_load_explicit(&g_running, memory_order_acquire);
}

double kf_engine_sample_rate(void) {
    return kf_engine_is_running() ? atomic_load_explicit(&g_rate, memory_order_relaxed) : 0.0;
}

static float clamp01(float value) {
    if (!(value >= 0.0f)) {
        return 0.0f; /* Also maps NaN to 0. */
    }
    return value > 1.0f ? 1.0f : value;
}

void kf_engine_set_volume(float volume) {
    atomic_store_explicit(&g_state.volume, clamp01(volume), memory_order_relaxed);
}

void kf_engine_set_muted(bool muted) {
    atomic_store_explicit(&g_state.muted, muted, memory_order_relaxed);
}

void kf_engine_set_stereo_width(float width) {
    atomic_store_explicit(&g_state.stereo_width, clamp01(width), memory_order_relaxed);
}

void kf_engine_set_big_enter(bool enabled) {
    atomic_store_explicit(&g_state.big_enter, enabled, memory_order_relaxed);
}

void kf_engine_set_change_handler(kf_engine_change_cb callback, void *ctx) {
    g_change_cb = callback;
    g_change_ctx = ctx;
}

/* ------------------------------------------------------------------ pack handoff */

static void free_released(void) {
    kf_pack *pack = NULL;
    while (kf_pack_queue_pop(&g_state.released, &pack)) {
        kf_pack_free(pack);
    }
}

static void sleep_ns(uint64_t ns) {
    struct timespec ts = {(time_t)(ns / 1000000000ULL), (long)(ns % 1000000000ULL)};
    while (nanosleep(&ts, &ts) != 0) {
    }
}

void kf_engine_set_pack(kf_pack *pack) {
    const uint64_t target = atomic_load_explicit(&g_state.swaps, memory_order_acquire) + 1u;
    kf_pack *unused = atomic_exchange_explicit(&g_state.pending, pack != NULL ? pack : KF_PACK_NONE,
                                               memory_order_acq_rel);
    if (unused != NULL && unused != KF_PACK_NONE) {
        kf_pack_free(unused); /* Published earlier but never reached the audio thread. */
    }
    if (!kf_engine_is_running()) {
        kf_engine_render_settle();
    } else {
        for (uint64_t waited = 0u; waited < KF_PACK_SWAP_TIMEOUT_NS;
             waited += KF_PACK_SWAP_POLL_NS) {
            if (atomic_load_explicit(&g_state.retirements, memory_order_acquire) >= target) {
                break;
            }
            sleep_ns(KF_PACK_SWAP_POLL_NS);
        }
        /* On a timeout the old pack stays queued, and the next call or kf_engine_stop()
         * frees it: never freed while it may still play. */
    }
    free_released();
}

/* ------------------------------------------------------------------ counters */

static uint64_t hist_percentile(const uint32_t *hist, uint64_t total, double q, uint64_t max_ns) {
    if (total == 0u) {
        return 0u;
    }
    const uint64_t rank = (uint64_t)((double)total * q + 0.999999);
    uint64_t seen = 0u;
    for (uint32_t i = 0u; i < KF_CALLBACK_HIST_BUCKETS; ++i) {
        seen += hist[i];
        if (seen >= rank) {
            return (uint64_t)(i + 1u) * KF_CALLBACK_HIST_STEP_NS; /* Bucket's upper bound. */
        }
    }
    return max_ns;
}

void kf_engine_get_counters(kf_engine_counters *out) {
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof *out);
    out->requests_dropped = atomic_load_explicit(&g_state.dropped, memory_order_relaxed);
    out->requests_stale = atomic_load_explicit(&g_state.stale, memory_order_relaxed);
    out->voices_stolen = atomic_load_explicit(&g_state.stolen, memory_order_relaxed);
    out->callbacks = atomic_load_explicit(&g_state.callbacks, memory_order_relaxed);
    out->callback_overruns = atomic_load_explicit(&g_state.overruns, memory_order_relaxed);
    out->max_callback_ns = atomic_load_explicit(&g_state.max_callback_ns, memory_order_relaxed);
    out->start_latency_ns = atomic_load_explicit(&g_state.start_latency_ns, memory_order_relaxed);
    out->granted_buffer_frames = g_granted_frames;
    out->sample_rate = kf_engine_sample_rate();
    uint32_t hist[KF_CALLBACK_HIST_BUCKETS + 1u];
    uint64_t total = 0u;
    for (uint32_t i = 0u; i <= KF_CALLBACK_HIST_BUCKETS; ++i) {
        hist[i] = atomic_load_explicit(&g_state.callback_hist[i], memory_order_relaxed);
        total += hist[i];
    }
    out->callback_p50_ns = hist_percentile(hist, total, 0.50, out->max_callback_ns);
    out->callback_p99_ns = hist_percentile(hist, total, 0.99, out->max_callback_ns);
}

/* ------------------------------------------------------------------ CoreAudio */

#if defined(__APPLE__)

static AudioComponentInstance g_unit;
static AudioObjectID g_device = kAudioObjectUnknown;
static atomic_bool g_change_pending;

static const AudioObjectPropertyAddress k_default_output = {
    kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMain};
static const AudioObjectPropertyAddress k_nominal_rate = {kAudioDevicePropertyNominalSampleRate,
                                                          kAudioObjectPropertyScopeGlobal,
                                                          kAudioObjectPropertyElementMain};

static void notify_change(void *ctx) {
    (void)ctx;
    atomic_store_explicit(&g_change_pending, false, memory_order_relaxed);
    if (kf_engine_is_running() && g_change_cb != NULL) {
        g_change_cb(g_change_ctx);
    }
}

/* HAL notification thread: hop to the main queue once per burst of changes. */
static OSStatus on_property(AudioObjectID object, UInt32 count,
                            const AudioObjectPropertyAddress *addresses, void *ctx) {
    (void)object;
    (void)count;
    (void)addresses;
    (void)ctx;
    if (!atomic_exchange_explicit(&g_change_pending, true, memory_order_relaxed)) {
        dispatch_async_f(dispatch_get_main_queue(), NULL, notify_change);
    }
    return noErr;
}

static AudioObjectID default_output_device(void) {
    AudioObjectID device = kAudioObjectUnknown;
    UInt32 size = sizeof device;
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &k_default_output, 0u, NULL, &size,
                                   &device) != noErr) {
        return kAudioObjectUnknown;
    }
    return device;
}

kf_status kf_engine_default_output_rate(double *rate) {
    if (rate == NULL) {
        return KF_ERR_INVALID_ARGUMENT;
    }
    const AudioObjectID device = default_output_device();
    Float64 nominal = 0.0;
    UInt32 size = sizeof nominal;
    if (device == kAudioObjectUnknown ||
        AudioObjectGetPropertyData(device, &k_nominal_rate, 0u, NULL, &size, &nominal) != noErr ||
        !(nominal > 0.0)) {
        return KF_ERR_AUDIO;
    }
    *rate = nominal;
    return KF_OK;
}

void kf_engine_output_device_name(char *buf, size_t len) {
    if (buf == NULL || len == 0u) {
        return;
    }
    buf[0] = '\0';
    const AudioObjectID device =
        g_device != kAudioObjectUnknown ? g_device : default_output_device();
    const AudioObjectPropertyAddress name = {
        kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    CFStringRef text = NULL;
    UInt32 size = sizeof(CFStringRef);
    if (device != kAudioObjectUnknown &&
        AudioObjectGetPropertyData(device, &name, 0u, NULL, &size, (void *)&text) == noErr &&
        text != NULL) {
        (void)CFStringGetCString(text, buf, (CFIndex)len, kCFStringEncodingUTF8);
        CFRelease(text);
    }
}

static void dispose_unit(void) {
    if (g_unit != NULL) {
        (void)AudioUnitUninitialize(g_unit);
        (void)AudioComponentInstanceDispose(g_unit);
        g_unit = NULL;
    }
}

static void remove_listeners(void) {
    (void)AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &k_default_output,
                                            on_property, NULL);
    if (g_device != kAudioObjectUnknown) {
        (void)AudioObjectRemovePropertyListener(g_device, &k_nominal_rate, on_property, NULL);
    }
}

static kf_status fail(const char *step, OSStatus status) {
    (void)step;
    (void)status; /* Kept for a debugger; the app reports KF_ERR_AUDIO in diagnostics. */
    dispose_unit();
    return KF_ERR_AUDIO;
}

kf_status kf_engine_start(void) {
    if (kf_engine_is_running()) {
        return KF_OK;
    }
    const AudioComponentDescription desc = {kAudioUnitType_Output, kAudioUnitSubType_DefaultOutput,
                                            kAudioUnitManufacturer_Apple, 0u, 0u};
    AudioComponent component = AudioComponentFindNext(NULL, &desc);
    if (component == NULL) {
        return KF_ERR_AUDIO;
    }
    OSStatus status = AudioComponentInstanceNew(component, &g_unit);
    if (status != noErr) {
        g_unit = NULL;
        return KF_ERR_AUDIO;
    }

    /* The hardware side of the unit carries the device's rate: render at it, so the mixer
     * and the pack never resample. */
    AudioStreamBasicDescription hardware;
    UInt32 size = sizeof hardware;
    status = AudioUnitGetProperty(g_unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output,
                                  0u, &hardware, &size);
    double rate = status == noErr ? hardware.mSampleRate : 0.0;
    if (!(rate > 0.0) && kf_engine_default_output_rate(&rate) != KF_OK) {
        return fail("sample rate", status);
    }
    const AudioStreamBasicDescription format = {
        .mSampleRate = rate,
        .mFormatID = kAudioFormatLinearPCM,
        .mFormatFlags =
            (UInt32)kAudioFormatFlagsNativeFloatPacked | (UInt32)kAudioFormatFlagIsNonInterleaved,
        .mBytesPerPacket = sizeof(float),
        .mFramesPerPacket = 1u,
        .mBytesPerFrame = sizeof(float),
        .mChannelsPerFrame = KF_MAX_OUTPUT_CHANNELS,
        .mBitsPerChannel = 32u,
    };
    status = AudioUnitSetProperty(g_unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input,
                                  0u, &format, sizeof format);
    if (status != noErr) {
        return fail("stream format", status);
    }
    const AURenderCallbackStruct callback = {kf_engine_output_callback, NULL};
    status = AudioUnitSetProperty(g_unit, kAudioUnitProperty_SetRenderCallback,
                                  kAudioUnitScope_Input, 0u, &callback, sizeof callback);
    if (status != noErr) {
        return fail("render callback", status);
    }
    /* Per-process request on this unit only; the device grants what it can. */
    const UInt32 frames = KF_IO_BUFFER_FRAMES;
    (void)AudioUnitSetProperty(g_unit, kAudioDevicePropertyBufferFrameSize, kAudioUnitScope_Global,
                               0u, &frames, sizeof frames);
    status = AudioUnitInitialize(g_unit);
    if (status != noErr) {
        return fail("initialize", status);
    }
    UInt32 granted = 0u;
    size = sizeof granted;
    g_granted_frames = AudioUnitGetProperty(g_unit, kAudioDevicePropertyBufferFrameSize,
                                            kAudioUnitScope_Global, 0u, &granted, &size) == noErr
                           ? granted
                           : 0u;
    AudioObjectID device = kAudioObjectUnknown;
    size = sizeof device;
    g_device = AudioUnitGetProperty(g_unit, kAudioOutputUnitProperty_CurrentDevice,
                                    kAudioUnitScope_Global, 0u, &device, &size) == noErr
                   ? device
                   : kAudioObjectUnknown;

    kf_engine_render_settle();
    kf_engine_render_reset(rate);
    reset_counters();
    atomic_store_explicit(&g_rate, rate, memory_order_relaxed);
    (void)AudioObjectAddPropertyListener(kAudioObjectSystemObject, &k_default_output, on_property,
                                         NULL);
    if (g_device != kAudioObjectUnknown) {
        (void)AudioObjectAddPropertyListener(g_device, &k_nominal_rate, on_property, NULL);
    }
    atomic_store_explicit(&g_state.start_ns, kf_time_now_ns(), memory_order_relaxed);
    atomic_store_explicit(&g_running, true, memory_order_release);
    status = AudioOutputUnitStart(g_unit);
    if (status != noErr) {
        atomic_store_explicit(&g_running, false, memory_order_release);
        remove_listeners();
        g_device = kAudioObjectUnknown;
        return fail("start", status);
    }
    free_released();
    return KF_OK;
}

void kf_engine_stop(void) {
    if (!kf_engine_is_running()) {
        return;
    }
    (void)AudioOutputUnitStop(g_unit); /* Synchronous: no callback runs after it returns. */
    remove_listeners();
    dispose_unit();
    g_device = kAudioObjectUnknown;
    atomic_store_explicit(&g_running, false, memory_order_release);
    kf_engine_render_settle();
    free_released();
}

#else /* Host builds: the mixer is tested directly; there is no audio device. */

kf_status kf_engine_default_output_rate(double *rate) {
    (void)rate;
    return KF_ERR_AUDIO;
}

void kf_engine_output_device_name(char *buf, size_t len) {
    if (buf != NULL && len > 0u) {
        buf[0] = '\0';
    }
}

kf_status kf_engine_start(void) {
    return KF_ERR_AUDIO;
}

void kf_engine_stop(void) {
    kf_engine_render_settle();
    free_released();
}

#endif
