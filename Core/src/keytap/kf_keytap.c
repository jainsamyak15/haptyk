// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/* Event-tap lifecycle (main thread). Routing lives in kf_keytap_rt.c. */

#include <kf/kf_keytap.h>

#include <string.h>

bool kf_keytap_has_permission(void) {
    /* TODO(M1): return CGPreflightListenEventAccess(); */
    return false;
}

void kf_keytap_request_permission(void) {
    /* TODO(M1): (void)CGRequestListenEventAccess(); */
}

kf_status kf_keytap_start(kf_key_cb callback, void *ctx, bool active_mode) {
    (void)callback;
    (void)ctx;
    (void)active_mode;
    /*
     * TODO(M1): create the tap (key down, key up, flags changed; listen-only unless
     * active_mode) on its own pthread with a CFRunLoop at QOS_CLASS_USER_INTERACTIVE.
     * In the callback: re-enable on kCGEventTapDisabledByTimeout/UserInput; drop
     * auto-repeat unless enabled; derive modifier up/down from flag changes; build the
     * event with kf_keymap_make_event(); call kf_keytap_dispatch(); then `callback`.
     */
    return KF_ERR_NOT_IMPLEMENTED;
}

void kf_keytap_stop(void) {
    /* TODO(M1): invalidate the tap, stop the run loop and join the thread. */
}

void kf_keytap_get_counters(kf_keytap_counters *out) {
    if (out != NULL) {
        memset(out, 0, sizeof *out);
    }
}
