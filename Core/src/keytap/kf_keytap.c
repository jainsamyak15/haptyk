// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/* Event-tap lifecycle (main thread) and the keyboard watcher (keytap thread). The callbacks
 * are in kf_keytap_rt.c.
 *
 * Timestamps: key events are stamped with kf_time_now_ns() when the callback runs, on the
 * clock every core module shares. CGEventGetTimestamp()'s units are a Milestone 0 question
 * that has not been measured (docs/SENSOR_NOTES.md), so it is not used. Arrival time is late
 * by the tap's delivery delay only, which matters for nothing until force detection runs. */

#include <kf/kf_keytap.h>

#include <string.h>

#include "keytap/kf_keytap_internal.h"

static kf_keytap_ctx g_ctx;

kf_keytap_ctx *kf_keytap_context(void) {
    return &g_ctx;
}

void kf_keytap_set_repeat_sounds(bool enabled) {
    atomic_store_explicit(&g_ctx.repeat_sounds, enabled, memory_order_relaxed);
}

uint32_t kf_keytap_external_keyboards(void) {
    return atomic_load_explicit(&g_ctx.external_keyboards, memory_order_relaxed);
}

void kf_keytap_get_counters(kf_keytap_counters *out) {
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof *out);
    out->events = atomic_load_explicit(&g_ctx.events, memory_order_relaxed);
    out->tap_timeouts = atomic_load_explicit(&g_ctx.tap_timeouts, memory_order_relaxed);
    out->autorepeat_dropped = atomic_load_explicit(&g_ctx.autorepeat_dropped, memory_order_relaxed);
}

#if defined(__APPLE__)

#include <pthread.h>

#include <ApplicationServices/ApplicationServices.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hidsystem/IOLLEvent.h>
#include <dispatch/dispatch.h>

_Static_assert(KF_FLAG_LEFT_CONTROL == NX_DEVICELCTLKEYMASK, "NX_DEVICELCTLKEYMASK");
_Static_assert(KF_FLAG_LEFT_SHIFT == NX_DEVICELSHIFTKEYMASK, "NX_DEVICELSHIFTKEYMASK");
_Static_assert(KF_FLAG_RIGHT_SHIFT == NX_DEVICERSHIFTKEYMASK, "NX_DEVICERSHIFTKEYMASK");
_Static_assert(KF_FLAG_LEFT_COMMAND == NX_DEVICELCMDKEYMASK, "NX_DEVICELCMDKEYMASK");
_Static_assert(KF_FLAG_RIGHT_COMMAND == NX_DEVICERCMDKEYMASK, "NX_DEVICERCMDKEYMASK");
_Static_assert(KF_FLAG_LEFT_OPTION == NX_DEVICELALTKEYMASK, "NX_DEVICELALTKEYMASK");
_Static_assert(KF_FLAG_RIGHT_OPTION == NX_DEVICERALTKEYMASK, "NX_DEVICERALTKEYMASK");
_Static_assert(KF_FLAG_RIGHT_CONTROL == NX_DEVICERCTLKEYMASK, "NX_DEVICERCTLKEYMASK");
_Static_assert(KF_FLAG_CAPS_LOCK == kCGEventFlagMaskAlphaShift, "kCGEventFlagMaskAlphaShift");
_Static_assert(KF_FLAG_FUNCTION == kCGEventFlagMaskSecondaryFn, "kCGEventFlagMaskSecondaryFn");

CGEventRef kf_keytap_on_event(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *ref);
void kf_keytap_on_report(void *ref, IOReturn result, void *sender, IOHIDValueRef value);

static pthread_t g_thread;
static atomic_bool g_running;
static atomic_bool g_stop;
static dispatch_semaphore_t g_ready;
static kf_status g_start_status;
static bool g_active_mode;

bool kf_keytap_has_permission(void) {
    return CGPreflightListenEventAccess();
}

void kf_keytap_request_permission(void) {
    (void)CGRequestListenEventAccess();
}

bool kf_keytap_is_running(void) {
    return atomic_load_explicit(&g_running, memory_order_acquire);
}

/* ------------------------------------------------------------------ keyboards */

static bool device_is_builtin(IOHIDDeviceRef device) {
    const CFTypeRef value = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDBuiltInKey));
    if (value != NULL && CFGetTypeID(value) == CFBooleanGetTypeID()) {
        return CFBooleanGetValue((CFBooleanRef)value);
    }
    if (value != NULL && CFGetTypeID(value) == CFNumberGetTypeID()) {
        int flag = 0;
        return CFNumberGetValue((CFNumberRef)value, kCFNumberIntType, &flag) && flag != 0;
    }
    return false;
}

/* Report times are needed only to tell keyboards apart, so keyboards are opened only while an
 * external one is connected: with the built-in keyboard alone this costs nothing per key. */
static void update_report_watch(kf_keytap_ctx *ctx) {
    IOHIDManagerRef manager = (IOHIDManagerRef)ctx->keyboards;
    const bool want = atomic_load_explicit(&ctx->external_keyboards, memory_order_relaxed) > 0u;
    if (want && !ctx->values_open) {
        ctx->values_open = IOHIDManagerOpen(manager, kIOHIDOptionsTypeNone) == kIOReturnSuccess;
    } else if (!want && ctx->values_open) {
        (void)IOHIDManagerClose(manager, kIOHIDOptionsTypeNone);
        ctx->values_open = false;
        ctx->last_builtin_ns = 0u;
        ctx->last_external_ns = 0u;
    }
}

static void count_external(kf_keytap_ctx *ctx) {
    uint32_t external = 0u;
    for (uint32_t i = 0u; i < ctx->device_count; ++i) {
        external += ctx->device_builtin[i] ? 0u : 1u;
    }
    atomic_store_explicit(&ctx->external_keyboards, external, memory_order_relaxed);
    update_report_watch(ctx);
}

static void on_keyboard_added(void *ref, IOReturn result, void *sender, IOHIDDeviceRef device) {
    (void)result;
    (void)sender;
    kf_keytap_ctx *ctx = ref;
    if (ctx->device_count < KF_KEYTAP_MAX_KEYBOARDS) {
        ctx->devices[ctx->device_count] = (void *)device;
        ctx->device_builtin[ctx->device_count] = device_is_builtin(device);
        ctx->device_count++;
    }
    count_external(ctx);
}

static void on_keyboard_removed(void *ref, IOReturn result, void *sender, IOHIDDeviceRef device) {
    (void)result;
    (void)sender;
    kf_keytap_ctx *ctx = ref;
    for (uint32_t i = 0u; i < ctx->device_count; ++i) {
        if (ctx->devices[i] == (void *)device) {
            ctx->device_count--;
            ctx->devices[i] = ctx->devices[ctx->device_count];
            ctx->device_builtin[i] = ctx->device_builtin[ctx->device_count];
            break;
        }
    }
    count_external(ctx);
}

static CFDictionaryRef make_usage_dict(CFStringRef page_key, int page, CFStringRef usage_key,
                                       int usage) {
    CFNumberRef numbers[2] = {CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &page),
                              CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usage)};
    const void *keys[2] = {page_key, usage_key};
    const CFIndex count = usage_key != NULL ? 2 : 1;
    CFDictionaryRef dict =
        CFDictionaryCreate(kCFAllocatorDefault, keys, (const void **)numbers, count,
                           &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFRelease(numbers[0]);
    CFRelease(numbers[1]);
    return dict;
}

static void start_keyboard_watch(kf_keytap_ctx *ctx, CFRunLoopRef loop) {
    IOHIDManagerRef manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    if (manager == NULL) {
        return; /* Without it, every key counts as built-in: the v1 default. */
    }
    CFDictionaryRef devices =
        make_usage_dict(CFSTR(kIOHIDDeviceUsagePageKey), 0x01, CFSTR(kIOHIDDeviceUsageKey), 0x06);
    CFDictionaryRef keys = make_usage_dict(CFSTR(kIOHIDElementUsagePageKey), 0x07, NULL, 0);
    IOHIDManagerSetDeviceMatching(manager, devices);
    IOHIDManagerSetInputValueMatching(manager, keys);
    CFRelease(devices);
    CFRelease(keys);
    ctx->keyboards = (void *)manager;
    IOHIDManagerRegisterDeviceMatchingCallback(manager, on_keyboard_added, ctx);
    IOHIDManagerRegisterDeviceRemovalCallback(manager, on_keyboard_removed, ctx);
    IOHIDManagerRegisterInputValueCallback(manager, kf_keytap_on_report, ctx);
    IOHIDManagerScheduleWithRunLoop(manager, loop, kCFRunLoopDefaultMode);
}

static void stop_keyboard_watch(kf_keytap_ctx *ctx, CFRunLoopRef loop) {
    IOHIDManagerRef manager = (IOHIDManagerRef)ctx->keyboards;
    if (manager == NULL) {
        return;
    }
    IOHIDManagerUnscheduleFromRunLoop(manager, loop, kCFRunLoopDefaultMode);
    if (ctx->values_open) {
        (void)IOHIDManagerClose(manager, kIOHIDOptionsTypeNone);
        ctx->values_open = false;
    }
    CFRelease(manager);
    ctx->keyboards = NULL;
    ctx->device_count = 0u;
    atomic_store_explicit(&ctx->external_keyboards, 0u, memory_order_relaxed);
}

/* ------------------------------------------------------------------ thread */

static void stop_timer_fired(CFRunLoopTimerRef timer, void *info) {
    (void)timer;
    (void)info;
    CFRunLoopStop(CFRunLoopGetCurrent());
}

static void *tap_thread(void *arg) {
    kf_keytap_ctx *ctx = arg;
    (void)pthread_setname_np("com.keyfeel.keytap");
    CFRunLoopRef loop = CFRunLoopGetCurrent();
    const CGEventMask mask = CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp) |
                             CGEventMaskBit(kCGEventFlagsChanged);
    CFMachPortRef tap =
        CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap,
                         g_active_mode ? kCGEventTapOptionDefault : kCGEventTapOptionListenOnly,
                         mask, kf_keytap_on_event, ctx);
    if (tap == NULL) {
        g_start_status = KF_ERR_NO_PERMISSION;
        dispatch_semaphore_signal(g_ready);
        return NULL;
    }
    CFRunLoopSourceRef source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, tap, 0);
    CFRunLoopAddSource(loop, source, kCFRunLoopCommonModes);
    ctx->tap = (void *)tap;
    ctx->loop = (void *)loop;
    ctx->prev_flags = (uint64_t)CGEventSourceFlagsState(kCGEventSourceStateCombinedSessionState);
    start_keyboard_watch(ctx, loop);
    g_start_status = KF_OK;
    dispatch_semaphore_signal(g_ready);

    /* Waits at no cost: the run loop sleeps until a key, a keyboard change or the stop. */
    while (!atomic_load_explicit(&g_stop, memory_order_acquire)) {
        (void)CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0e10, false);
    }

    stop_keyboard_watch(ctx, loop);
    CGEventTapEnable(tap, false);
    CFRunLoopRemoveSource(loop, source, kCFRunLoopCommonModes);
    CFMachPortInvalidate(tap);
    CFRelease(source);
    CFRelease(tap);
    ctx->tap = NULL;
    ctx->loop = NULL;
    return NULL;
}

kf_status kf_keytap_start(kf_key_cb callback, void *ctx, bool active_mode) {
    if (kf_keytap_is_running()) {
        return KF_ERR_STATE;
    }
    g_ctx.callback = callback;
    g_ctx.callback_ctx = ctx;
    g_active_mode = active_mode;
    atomic_store_explicit(&g_stop, false, memory_order_relaxed);
    g_ready = dispatch_semaphore_create(0);
    if (g_ready == NULL) {
        return KF_ERR_NO_MEMORY;
    }
    pthread_attr_t attr;
    (void)pthread_attr_init(&attr);
    (void)pthread_attr_set_qos_class_np(&attr, QOS_CLASS_USER_INTERACTIVE, 0);
    const int created = pthread_create(&g_thread, &attr, tap_thread, &g_ctx);
    (void)pthread_attr_destroy(&attr);
    if (created != 0) {
        dispatch_release(g_ready);
        return KF_ERR_NO_MEMORY;
    }
    (void)dispatch_semaphore_wait(g_ready, DISPATCH_TIME_FOREVER);
    dispatch_release(g_ready);
    if (g_start_status != KF_OK) {
        (void)pthread_join(g_thread, NULL);
        return g_start_status;
    }
    atomic_store_explicit(&g_running, true, memory_order_release);
    return KF_OK;
}

void kf_keytap_stop(void) {
    if (!kf_keytap_is_running()) {
        return;
    }
    atomic_store_explicit(&g_stop, true, memory_order_release);
    /* A timer that fires at once wakes the loop even if it has not started running yet, where
     * CFRunLoopStop alone would be lost. */
    CFRunLoopTimerRef timer = CFRunLoopTimerCreate(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent(),
                                                   0.0, 0u, 0, stop_timer_fired, NULL);
    CFRunLoopAddTimer((CFRunLoopRef)g_ctx.loop, timer, kCFRunLoopDefaultMode);
    CFRunLoopWakeUp((CFRunLoopRef)g_ctx.loop);
    (void)pthread_join(g_thread, NULL);
    CFRelease(timer);
    atomic_store_explicit(&g_running, false, memory_order_release);
}

#else /* Host builds: routing and the pure rules are tested; there is no event tap. */

bool kf_keytap_has_permission(void) {
    return false;
}

void kf_keytap_request_permission(void) {}

kf_status kf_keytap_start(kf_key_cb callback, void *ctx, bool active_mode) {
    (void)callback;
    (void)ctx;
    (void)active_mode;
    return KF_ERR_NO_PERMISSION;
}

void kf_keytap_stop(void) {}

bool kf_keytap_is_running(void) {
    return false;
}

#endif
