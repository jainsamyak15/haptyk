// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/* Real-time translation unit: only RT-safe code (checked by `make check-rt`).
 * The event-tap and HID callbacks, routing, and the pure rules they use. */

#include <kf/kf_config.h>
#include <kf/kf_engine.h>
#include <kf/kf_keymap.h>
#include <kf/kf_sensor.h>
#include <kf/kf_time.h>

#include "internal/kf_rt.h"
#include "keymap/kf_vk.h"
#include "keytap/kf_keytap_internal.h"

KF_RT_SAFE void kf_keytap_dispatch(const kf_key_event *event) {
    if (event->is_down && event->builtin && kf_sensor_state_now(event->t_ns) == KF_SENSOR_LIVE &&
        kf_sensor_push_pending(event)) {
        return; /* The sensor thread measures its force and queues the voices. */
    }
    (void)kf_emit_keystroke(event, KF_DEFAULT_VELOCITY, KF_QUEUE_KEYTAP);
}

KF_RT_SAFE bool kf_keytap_modifier_change(uint16_t keycode, uint64_t prev_flags, uint64_t flags,
                                          bool *is_down) {
    uint64_t mask;
    switch (keycode) {
    case KF_VK_SHIFT:
        mask = KF_FLAG_LEFT_SHIFT;
        break;
    case KF_VK_RIGHT_SHIFT:
        mask = KF_FLAG_RIGHT_SHIFT;
        break;
    case KF_VK_CONTROL:
        mask = KF_FLAG_LEFT_CONTROL;
        break;
    case KF_VK_RIGHT_CONTROL:
        mask = KF_FLAG_RIGHT_CONTROL;
        break;
    case KF_VK_OPTION:
        mask = KF_FLAG_LEFT_OPTION;
        break;
    case KF_VK_RIGHT_OPTION:
        mask = KF_FLAG_RIGHT_OPTION;
        break;
    case KF_VK_COMMAND:
        mask = KF_FLAG_LEFT_COMMAND;
        break;
    case KF_VK_RIGHT_COMMAND:
        mask = KF_FLAG_RIGHT_COMMAND;
        break;
    case KF_VK_FUNCTION:
        mask = KF_FLAG_FUNCTION;
        break;
    case KF_VK_CAPS_LOCK:
        if (((prev_flags ^ flags) & KF_FLAG_CAPS_LOCK) == 0u) {
            return false;
        }
        *is_down = true; /* Each toggle is one press. */
        return true;
    default:
        return false;
    }
    if (((prev_flags ^ flags) & mask) == 0u) {
        return false;
    }
    *is_down = (flags & mask) != 0u;
    return true;
}

static KF_RT_SAFE uint64_t distance(uint64_t a, uint64_t b) {
    return a > b ? a - b : b - a;
}

KF_RT_SAFE bool kf_keytap_is_builtin(uint64_t t_ns, uint64_t last_builtin_ns,
                                     uint64_t last_external_ns, uint32_t external_keyboards) {
    if (external_keyboards == 0u) {
        return true;
    }
    if (last_builtin_ns == 0u || distance(t_ns, last_builtin_ns) > KF_BUILTIN_MATCH_NS) {
        return false; /* Unmatched: no force detection, the safe side. */
    }
    return last_external_ns == 0u || distance(t_ns, last_external_ns) > KF_BUILTIN_MATCH_NS ||
           distance(t_ns, last_builtin_ns) <= distance(t_ns, last_external_ns);
}

KF_RT_SAFE void kf_keytap_handle_key(kf_keytap_ctx *ctx, uint16_t keycode, bool is_down,
                                     uint64_t t_ns) {
    const bool builtin =
        kf_keytap_is_builtin(t_ns, ctx->last_builtin_ns, ctx->last_external_ns,
                             atomic_load_explicit(&ctx->external_keyboards, memory_order_relaxed));
    /* The key code ends here: kf_key_event has no field for it. */
    const kf_key_event event = kf_keymap_make_event(keycode, is_down, t_ns, builtin);
    atomic_store_explicit(&ctx->events,
                          atomic_load_explicit(&ctx->events, memory_order_relaxed) + 1u,
                          memory_order_relaxed);
    kf_keytap_dispatch(&event);
    if (ctx->callback != NULL) {
        ctx->callback(&event, ctx->callback_ctx);
    }
}

#if defined(__APPLE__)

#include <ApplicationServices/ApplicationServices.h>
#include <IOKit/hid/IOHIDManager.h>

/* Declared here, not in the header, so the header stays framework-free. */
CGEventRef kf_keytap_on_event(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *ref);
void kf_keytap_on_report(void *ref, IOReturn result, void *sender, IOHIDValueRef value);

static KF_RT_SAFE void bump(_Atomic(uint64_t) *counter) {
    atomic_store_explicit(counter, atomic_load_explicit(counter, memory_order_relaxed) + 1u,
                          memory_order_relaxed);
}

KF_RT_SAFE CGEventRef kf_keytap_on_event(CGEventTapProxy proxy, CGEventType type, CGEventRef event,
                                         void *ref) {
    (void)proxy;
    const uint64_t now = kf_time_now_ns(); /* Our clock at arrival: see kf_keytap.c. */
    kf_keytap_ctx *ctx = ref;
    if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
        CGEventTapEnable((CFMachPortRef)ctx->tap, true);
        bump(&ctx->tap_timeouts);
        return event;
    }
    const uint16_t keycode = (uint16_t)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
    bool is_down = false;
    if (type == kCGEventFlagsChanged) {
        const uint64_t flags = (uint64_t)CGEventGetFlags(event);
        const bool changed = kf_keytap_modifier_change(keycode, ctx->prev_flags, flags, &is_down);
        ctx->prev_flags = flags;
        if (!changed) {
            return event;
        }
    } else if (type == kCGEventKeyDown || type == kCGEventKeyUp) {
        if (CGEventGetIntegerValueField(event, kCGKeyboardEventAutorepeat) != 0 &&
            !atomic_load_explicit(&ctx->repeat_sounds, memory_order_relaxed)) {
            bump(&ctx->autorepeat_dropped);
            return event;
        }
        is_down = type == kCGEventKeyDown;
    } else {
        return event;
    }
    kf_keytap_handle_key(ctx, keycode, is_down, now);
    return event; /* Passed through untouched. */
}

/* A keyboard's HID report: records only when, and whether from the built-in keyboard. */
KF_RT_SAFE void kf_keytap_on_report(void *ref, IOReturn result, void *sender, IOHIDValueRef value) {
    (void)result;
    (void)sender;
    const uint64_t now = kf_time_now_ns();
    kf_keytap_ctx *ctx = ref;
    const IOHIDDeviceRef device = IOHIDElementGetDevice(IOHIDValueGetElement(value));
    for (uint32_t i = 0u; i < ctx->device_count; ++i) {
        if (ctx->devices[i] == (void *)device) {
            if (ctx->device_builtin[i]) {
                ctx->last_builtin_ns = now;
            } else {
                ctx->last_external_ns = now;
            }
            return;
        }
    }
}

#endif
