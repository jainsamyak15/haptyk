// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/**
 * @file kf_keytap.h
 * @brief System-wide keystroke capture through a CGEventTap on its own thread.
 *
 * Listen-only unless active_mode is requested (Shout Mode). The tap respects
 * Secure Input: in password fields it receives nothing, by design.
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

/** Starts the tap thread. `callback` may be NULL. Main thread. */
kf_status kf_keytap_start(kf_key_cb callback, void *ctx, bool active_mode);

/** Stops and joins the tap thread. Main thread. */
void kf_keytap_stop(void);

/** Diagnostics counters. */
typedef struct kf_keytap_counters {
    uint64_t events;             /**< Key events seen. */
    uint64_t tap_timeouts;       /**< Times macOS disabled the tap and it was re-enabled. */
    uint64_t autorepeat_dropped; /**< Auto-repeat events ignored. */
} kf_keytap_counters;

/** Snapshot of the counters. Any thread. */
void kf_keytap_get_counters(kf_keytap_counters *out);

#ifdef __cplusplus
}
#endif

#endif /* KF_KEYTAP_H */
