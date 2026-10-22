// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/**
 * @file kf_keytap.h
 * @brief System-wide keystroke capture through a CGEventTap on its own thread.
 *
 * Listen-only unless active_mode is requested (Shout Mode). The event mask is key down, key
 * up and flags changed: never mouse events. The tap respects Secure Input: in password fields
 * it receives nothing, by design. Key codes are used for the key-map lookup and discarded.
 *
 * Built-in detection (spec, keytap.c): with no external keyboard connected every key is
 * built-in. With one connected, a key is built-in when the built-in keyboard reported a key
 * within KF_BUILTIN_MATCH_NS of it and nearer in time than any external keyboard. Only
 * report times are recorded, never which key.
 */
#ifndef KF_KEYTAP_H
#define KF_KEYTAP_H

#include <stdbool.h>
#include <stdint.h>

#include <kf/kf_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Called on the keytap thread after the core has routed the key. Must return within 50 us. */
typedef void (*kf_key_cb)(const kf_key_event *event, void *ctx);

/** True when Input Monitoring is granted (CGPreflightListenEventAccess). Main thread. */
bool kf_keytap_has_permission(void);

/** Shows the system Input Monitoring prompt (CGRequestListenEventAccess). Main thread. */
void kf_keytap_request_permission(void);

/**
 * Starts the tap thread at user-interactive QoS and returns once the tap exists.
 * KF_ERR_NO_PERMISSION when macOS refuses the tap (Input Monitoring missing, or
 * Accessibility for active_mode); KF_ERR_STATE when already running. `callback` may be NULL.
 * Main thread.
 */
kf_status kf_keytap_start(kf_key_cb callback, void *ctx, bool active_mode);

/** Stops and joins the tap thread. Safe when not running. Main thread. */
void kf_keytap_stop(void);

/** Any thread. */
bool kf_keytap_is_running(void);

/** "Sound on key repeat": play auto-repeat events too (default off). Any thread. */
void kf_keytap_set_repeat_sounds(bool enabled);

/** "Sound on key release": play release sounds (default on). Any thread. */
void kf_keytap_set_release_sounds(bool enabled);

/** "Mute when typing on an external keyboard": keys not from the built-in keyboard stay
 * silent (default off). The callback still sees them. Any thread. */
void kf_keytap_set_mute_external(bool muted);

/** External keyboards connected now (0 until the tap runs). Any thread. */
uint32_t kf_keytap_external_keyboards(void);

/** Diagnostics counters. */
typedef struct kf_keytap_counters {
    uint64_t events;             /**< Key events played. */
    uint64_t tap_timeouts;       /**< Times macOS disabled the tap and it was re-enabled. */
    uint64_t autorepeat_dropped; /**< Auto-repeat events ignored. */
} kf_keytap_counters;

/** Snapshot of the counters. Any thread. */
void kf_keytap_get_counters(kf_keytap_counters *out);

#ifdef __cplusplus
}
#endif

#endif /* KF_KEYTAP_H */
