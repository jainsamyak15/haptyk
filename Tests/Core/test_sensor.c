// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#include <kf/kf_engine.h>
#include <kf/kf_sensor.h>

#include "engine/kf_engine_internal.h"
#include "keytap/kf_keytap_internal.h"
#include "kf_test.h"
#include "sensor/kf_sensor_internal.h"

#define MS 1000000ULL

static kf_key_event key_down(uint64_t t_ns, bool builtin) {
    const kf_key_event event = {
        .t_ns = t_ns,
        .group = KF_GROUP_ALPHA,
        .row = KF_ROW_HOME,
        .pan = 0,
        .is_down = true,
        .builtin = builtin,
    };
    return event;
}

static void feed(uint64_t from_ms, uint64_t to_ms, uint64_t spike_ms, float spike_g) {
    for (uint64_t t = from_ms; t <= to_ms; ++t) {
        const kf_accel_sample s = {t * MS, 0.0f, 0.0f, 1.0f + (t == spike_ms ? spike_g : 0.0f)};
        kf_sensor_process_sample(&s);
    }
}

static uint32_t queued(kf_queue queue) {
    return kf_voice_queue_size_approx(kf_engine_voice_queue(queue));
}

static void reset_all(void) {
    KF_EXPECT(kf_engine_init() == KF_OK);
    kf_sensor_reset();
    kf_calibration cal = kf_calibration_default();
    cal.auto_adapt = false;
    KF_EXPECT(kf_sensor_set_calibration(&cal) == KF_OK);
}

void test_sensor_resolves_after_window(void) {
    reset_all();
    const kf_key_event down = key_down(500u * MS, true);
    KF_EXPECT(kf_sensor_push_pending(&down));

    feed(400u, 507u, 502u, 0.3f); /* Window closes at 508 ms. */
    KF_EXPECT(queued(KF_QUEUE_SENSOR) == 0u);
    feed(508u, 508u, 0u, 0.0f);
    KF_EXPECT(queued(KF_QUEUE_SENSOR) == 1u); /* Above slam: a single slam voice. */

    kf_voice_req req = {.t_ns = 0u};
    KF_EXPECT(kf_voice_queue_pop(kf_engine_voice_queue(KF_QUEUE_SENSOR), &req));
    KF_EXPECT(req.tier == KF_TIER_SLAM);
    KF_EXPECT_NEAR(req.gain, 1.0, 1e-5);
    KF_EXPECT_NEAR(kf_sensor_last_velocity(), 1.0, 1e-6);

    kf_sensor_counters counters;
    kf_sensor_get_counters(&counters);
    KF_EXPECT(counters.keys_resolved == 1u && counters.keys_defaulted == 0u);
    KF_EXPECT(counters.samples == 109u);
}

void test_sensor_flush_uses_default_velocity(void) {
    reset_all();
    const kf_key_event down = key_down(500u * MS, true);
    KF_EXPECT(kf_sensor_push_pending(&down));
    kf_sensor_stop(); /* No samples ever arrived: the key still plays. */
    KF_EXPECT(queued(KF_QUEUE_SENSOR) == 2u);
    KF_EXPECT_NEAR(kf_sensor_last_velocity(), KF_DEFAULT_VELOCITY, 1e-6);
    kf_sensor_counters counters;
    kf_sensor_get_counters(&counters);
    KF_EXPECT(counters.keys_defaulted == 1u);
}

void test_keytap_dispatch_routes_by_sensor_state(void) {
    reset_all();
    kf_pending_queue *pending = &kf_sensor_context()->pending;

    /* Sensor stopped: plays at once on queue A at the default velocity (two tier voices). */
    const kf_key_event first = key_down(1000u * MS, true);
    kf_keytap_dispatch(&first);
    KF_EXPECT(queued(KF_QUEUE_KEYTAP) == 2u);
    KF_EXPECT(kf_pending_queue_size_approx(pending) == 0u);

    /* Sensor live and fresh: built-in key-downs wait for the sensor thread. */
    kf_sensor_set_state(KF_SENSOR_LIVE);
    feed(1000u, 1000u, 0u, 0.0f);
    const kf_key_event builtin = key_down(1005u * MS, true);
    kf_keytap_dispatch(&builtin);
    KF_EXPECT(kf_pending_queue_size_approx(pending) == 1u);
    KF_EXPECT(queued(KF_QUEUE_KEYTAP) == 2u);

    /* External keyboard: no force detection, plays at once. */
    const kf_key_event external = key_down(1006u * MS, false);
    kf_keytap_dispatch(&external);
    KF_EXPECT(queued(KF_QUEUE_KEYTAP) == 4u);

    /* Key-up: release sound, at once. */
    kf_key_event up = key_down(1007u * MS, true);
    up.is_down = false;
    kf_keytap_dispatch(&up);
    KF_EXPECT(queued(KF_QUEUE_KEYTAP) == 5u);

    /* Sensor gone quiet for 100 ms: treated as stale, plays at once. */
    const kf_key_event late = key_down(1100u * MS, true);
    KF_EXPECT(kf_sensor_state_now(late.t_ns) == KF_SENSOR_STALE);
    kf_keytap_dispatch(&late);
    KF_EXPECT(queued(KF_QUEUE_KEYTAP) == 7u);
    KF_EXPECT(kf_pending_queue_size_approx(pending) == 1u);
    kf_sensor_stop();
}
