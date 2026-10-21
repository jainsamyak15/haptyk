// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/* Real-time translation unit: only RT-safe code (checked by `make check-rt`). */

#include <kf/kf_config.h>
#include <kf/kf_engine.h>
#include <kf/kf_sensor.h>

#include "internal/kf_rt.h"
#include "keytap/kf_keytap_internal.h"

KF_RT_SAFE void kf_keytap_dispatch(const kf_key_event *event) {
    if (event->is_down && event->builtin && kf_sensor_state_now(event->t_ns) == KF_SENSOR_LIVE &&
        kf_sensor_push_pending(event)) {
        return; /* The sensor thread measures its force and queues the voices. */
    }
    (void)kf_emit_keystroke(event, KF_DEFAULT_VELOCITY, KF_QUEUE_KEYTAP);
}
