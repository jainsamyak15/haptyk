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

static KF_RT_SAFE bool is_builtin_now(const kf_keytap_ctx *ctx, uint64_t t_ns) {
    return kf_keytap_is_builtin(
        t_ns, ctx->last_builtin_ns, ctx->last_external_ns,
        atomic_load_explicit(&ctx->external_keyboards, memory_order_relaxed));
}

KF_RT_SAFE void kf_keytap_handle_key(kf_keytap_ctx *ctx, uint16_t keycode, bool is_down,
                                     uint64_t t_ns) {
    const bool builtin = is_builtin_now(ctx, t_ns);
    /* The key code ends here: kf_key_event has no field for it. */
    const kf_key_event event = kf_keymap_make_event(keycode, is_down, t_ns, builtin);
    atomic_store_explicit(&ctx->events,
                          atomic_load_explicit(&ctx->events, memory_order_relaxed) + 1u,
                          memory_order_relaxed);
    const bool silent =
        (!is_down && atomic_load_explicit(&ctx->mute_release, memory_order_relaxed)) ||
        (!builtin && atomic_load_explicit(&ctx->mute_external, memory_order_relaxed));
    if (!silent) {
        kf_keytap_dispatch(&event);
    }
    if (ctx->callback != NULL) {
        ctx->callback(&event, ctx->callback_ctx);
    }
}

/* ------------------------------------------------------------------ Shout Mode */

#define LETTER(vk) (1ULL << (vk))
static const uint64_t k_letter_keys =
    LETTER(KF_VK_A) | LETTER(KF_VK_B) | LETTER(KF_VK_C) | LETTER(KF_VK_D) | LETTER(KF_VK_E) |
    LETTER(KF_VK_F) | LETTER(KF_VK_G) | LETTER(KF_VK_H) | LETTER(KF_VK_I) | LETTER(KF_VK_J) |
    LETTER(KF_VK_K) | LETTER(KF_VK_L) | LETTER(KF_VK_M) | LETTER(KF_VK_N) | LETTER(KF_VK_O) |
    LETTER(KF_VK_P) | LETTER(KF_VK_Q) | LETTER(KF_VK_R) | LETTER(KF_VK_S) | LETTER(KF_VK_T) |
    LETTER(KF_VK_U) | LETTER(KF_VK_V) | LETTER(KF_VK_W) | LETTER(KF_VK_X) | LETTER(KF_VK_Y) |
    LETTER(KF_VK_Z);
#undef LETTER

KF_RT_SAFE uint64_t kf_keytap_shout_flags(uint16_t keycode, uint64_t flags, float rolling_force) {
    const uint64_t shortcut = KF_FLAG_COMMAND | KF_FLAG_CONTROL | KF_FLAG_OPTION;
    if (keycode >= 64u || ((k_letter_keys >> keycode) & 1u) == 0u || (flags & shortcut) != 0u ||
        !(rolling_force >= KF_SHOUT_FORCE)) { /* Also false for NaN and "no force yet" (-1). */
        return flags;
    }
    return flags | KF_FLAG_SHIFT;
}

KF_RT_SAFE uint64_t kf_keytap_shout(kf_keytap_ctx *ctx, uint16_t keycode, uint64_t flags,
                                    uint64_t t_ns) {
    if (!atomic_load_explicit(&ctx->shout, memory_order_relaxed) ||
        kf_sensor_state_now(t_ns) != KF_SENSOR_LIVE || !is_builtin_now(ctx, t_ns)) {
        return flags;
    }
    const uint64_t shouted = kf_keytap_shout_flags(keycode, flags, kf_sensor_rolling_velocity());
    if (shouted != flags) {
        atomic_store_explicit(&ctx->shout_keys,
                              atomic_load_explicit(&ctx->shout_keys, memory_order_relaxed) + 1u,
                              memory_order_relaxed);
    }
    return shouted;
}

KF_RT_SAFE bool kf_keytap_note_timeout(kf_keytap_ctx *ctx, uint64_t t_ns) {
    if (!atomic_load_explicit(&ctx->shout, memory_order_relaxed)) {
        return false;
    }
    ctx->timeouts_ns[ctx->timeouts_next] = t_ns;
    ctx->timeouts_next = (ctx->timeouts_next + 1u) % KF_SHOUT_MAX_TIMEOUTS;
    /* Now the next slot holds the oldest of the last KF_SHOUT_MAX_TIMEOUTS (0: fewer yet). */
    const uint64_t oldest = ctx->timeouts_ns[ctx->timeouts_next];
    if (oldest == 0u || t_ns - oldest > KF_SHOUT_TIMEOUT_WINDOW_NS) {
        return false;
    }
    atomic_store_explicit(&ctx->shout, false, memory_order_relaxed);
    atomic_store_explicit(&ctx->shout_tripped, true, memory_order_release);
    return true;
}

KF_RT_SAFE bool kf_keytap_take_shout_tripped(void) {
    kf_keytap_ctx *ctx = kf_keytap_context();
    return atomic_load_explicit(&ctx->shout_tripped, memory_order_relaxed) &&
           atomic_exchange_explicit(&ctx->shout_tripped, false, memory_order_acquire);
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
        if (type == kCGEventTapDisabledByTimeout && ctx->active_mode) {
            (void)kf_keytap_note_timeout(ctx, now); /* The app learns of a trip at the next key. */
        }
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
        if (type == kCGEventKeyDown && ctx->active_mode) {
            /* Shout Mode, decided from earlier keys only: it never waits for sensor data. */
            const uint64_t flags = (uint64_t)CGEventGetFlags(event);
            const uint64_t shouted = kf_keytap_shout(ctx, keycode, flags, now);
            if (shouted != flags) {
                CGEventSetFlags(event, (CGEventFlags)shouted);
            }
        }
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
    return event; /* Passed through untouched, but for Shout Mode's Shift. */
}

/* A keyboard's HID report: records only when, and whether from the built-in keyboard. */
KF_RT_SAFE void kf_keytap_on_report(void *ref, IOReturn result, void *sender, IOHIDValueRef value) {
    (void)result;
    (void)sender;
    const uint64_t now = kf_time_now_ns();
    kf_keytap_ctx *ctx = ref;
    IOHIDDeviceRef device = IOHIDElementGetDevice(IOHIDValueGetElement(value));
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
