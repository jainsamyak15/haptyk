// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/**
 * @file kf_keytap_internal.h
 * @brief Keytap module internals. Not part of the public API.
 */
#ifndef KF_KEYTAP_INTERNAL_H
#define KF_KEYTAP_INTERNAL_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#include <kf/kf_keytap.h>
#include <kf/kf_types.h>

/* Event flag bits, mirrored so the modifier logic is host-testable. kf_keytap.c checks them
 * against the SDK (IOLLEvent.h, CGEventTypes.h) at compile time on macOS. */
enum {
    KF_FLAG_LEFT_CONTROL = 0x00000001,
    KF_FLAG_LEFT_SHIFT = 0x00000002,
    KF_FLAG_RIGHT_SHIFT = 0x00000004,
    KF_FLAG_LEFT_COMMAND = 0x00000008,
    KF_FLAG_RIGHT_COMMAND = 0x00000010,
    KF_FLAG_LEFT_OPTION = 0x00000020,
    KF_FLAG_RIGHT_OPTION = 0x00000040,
    KF_FLAG_RIGHT_CONTROL = 0x00002000,
    KF_FLAG_CAPS_LOCK = 0x00010000,
    KF_FLAG_FUNCTION = 0x00800000,
};

#define KF_KEYTAP_MAX_KEYBOARDS 8u

/** Keytap thread state. The counters and keyboard count are also read by other threads. */
typedef struct kf_keytap_ctx {
    kf_key_cb callback;
    void *callback_ctx;
    void *tap;       /* CFMachPortRef */
    void *loop;      /* CFRunLoopRef */
    void *keyboards; /* IOHIDManagerRef */
    uint64_t prev_flags;
    uint64_t last_builtin_ns;  /* Arrival of the latest report from the built-in keyboard... */
    uint64_t last_external_ns; /* ...and from any external one. Times only, never keys. */
    void *devices[KF_KEYTAP_MAX_KEYBOARDS];
    bool device_builtin[KF_KEYTAP_MAX_KEYBOARDS];
    uint32_t device_count;
    bool values_open; /* Keyboards opened for report times: only while an external one exists. */
    atomic_bool repeat_sounds;
    atomic_bool mute_release; /* Inverted, so the zeroed default plays release sounds. */
    atomic_bool mute_external;
    _Atomic(uint32_t) external_keyboards;
    _Atomic(uint64_t) events;
    _Atomic(uint64_t) tap_timeouts;
    _Atomic(uint64_t) autorepeat_dropped;
} kf_keytap_ctx;

kf_keytap_ctx *kf_keytap_context(void);

/**
 * Routes one classified key event: built-in key-downs go to the sensor thread while it is
 * LIVE; everything else plays at once on queue A (key-ups as the release sound, key-downs at
 * KF_DEFAULT_VELOCITY). Keytap thread; RT-safe.
 */
void kf_keytap_dispatch(const kf_key_event *event);

/**
 * Whether a flags-changed event for `keycode` is that modifier going down or up. Caps Lock
 * plays on each press (macOS reports the toggle, not the release). False when the event is
 * not about this key. RT-safe.
 */
bool kf_keytap_modifier_change(uint16_t keycode, uint64_t prev_flags, uint64_t flags,
                               bool *is_down);

/** The built-in rule, from report arrival times (0 = none yet). RT-safe. */
bool kf_keytap_is_builtin(uint64_t t_ns, uint64_t last_builtin_ns, uint64_t last_external_ns,
                          uint32_t external_keyboards);

/** Classifies, routes and reports one key event at `t_ns`. Keytap thread; RT-safe. */
void kf_keytap_handle_key(kf_keytap_ctx *ctx, uint16_t keycode, bool is_down, uint64_t t_ns);

#endif /* KF_KEYTAP_INTERNAL_H */
