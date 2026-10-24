// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#include <math.h>
#include <stdio.h>
#include <string.h>

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

/* ------------------------------------------------------------------ M2 */

static void put_i16(uint8_t *p, int16_t v) {
    memcpy(p, &v, sizeof v);
}

static int hex_nibble(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

static bool read_first_fixture_report(uint8_t out[KF_SENSOR_REPORT_BYTES]) {
    FILE *file = fopen("Tests/Fixtures/sensor/Mac17,3_macos-27.0_accel-rest.csv", "r");
    char line[256];
    bool found = false;
    if (file == NULL) {
        return false;
    }
    while (fgets(line, sizeof line, file) != NULL) {
        char *data = line;
        for (int field = 0; field < 3; ++field) {
            data = strchr(data, ',');
            if (data == NULL) {
                break;
            }
            data++;
        }
        if (data == NULL || strncmp(line, "#", 1u) == 0 || strstr(line, ",REPORT,") == NULL) {
            continue;
        }
        for (size_t i = 0u; i < KF_SENSOR_REPORT_BYTES; ++i) {
            const int hi = hex_nibble(data[2u * i]);
            const int lo = hex_nibble(data[2u * i + 1u]);
            if (hi < 0 || lo < 0) {
                break;
            }
            out[i] = (uint8_t)(((unsigned)hi << 4u) | (unsigned)lo);
            found = i + 1u == KF_SENSOR_REPORT_BYTES;
        }
        if (found) {
            break;
        }
    }
    (void)fclose(file);
    return found;
}

void test_sensor_decodes_measured_fixture(void) {
    uint8_t report[KF_SENSOR_REPORT_BYTES];
    kf_accel_sample sample;
    KF_EXPECT(read_first_fixture_report(report));
    KF_EXPECT(kf_sensor_decode_report(report, sizeof report, 99u, &sample));
    KF_EXPECT(sample.t_ns == 99u);
    KF_EXPECT_NEAR(sample.x, -3233.0 / 65536.0, 1e-6);
    KF_EXPECT_NEAR(sample.y, -14859.0 / 65536.0, 1e-6);
    KF_EXPECT_NEAR(sample.z, -63848.0 / 65536.0, 1e-6);
}

void test_sensor_decodes_each_layout(void) {
    uint8_t report[22];
    memset(report, 0xee, sizeof report);
    kf_accel_sample s;

    const kf_sensor_layout i16 = {22u, 4u, KF_SAMPLE_INT16, 1.0f / 16384.0f};
    put_i16(report + 4, 16384);
    put_i16(report + 6, -8192);
    put_i16(report + 8, 0);
    KF_EXPECT(kf_sensor_decode_with(&i16, report, 22u, 77u, &s));
    KF_EXPECT(s.t_ns == 77u);
    KF_EXPECT_NEAR(s.x, 1.0, 1e-6);
    KF_EXPECT_NEAR(s.y, -0.5, 1e-6);
    KF_EXPECT_NEAR(s.z, 0.0, 1e-6);

    const kf_sensor_layout i32 = {22u, 6u, KF_SAMPLE_INT32, 1.0f / 65536.0f};
    const int32_t values[3] = {65536, -32768, 131072};
    memcpy(report + 6, values, sizeof values); /* Unaligned offset on purpose. */
    KF_EXPECT(kf_sensor_decode_with(&i32, report, 22u, 1u, &s));
    KF_EXPECT_NEAR(s.x, 1.0, 1e-6);
    KF_EXPECT_NEAR(s.y, -0.5, 1e-6);
    KF_EXPECT_NEAR(s.z, 2.0, 1e-6);

    const kf_sensor_layout f32 = {22u, 10u, KF_SAMPLE_FLOAT32, 1.0f};
    const float floats[3] = {0.25f, 0.5f, -1.0f};
    memcpy(report + 10, floats, sizeof floats);
    KF_EXPECT(kf_sensor_decode_with(&f32, report, 22u, 1u, &s));
    KF_EXPECT_NEAR(s.z, -1.0, 1e-6);
    const float nan = NAN;
    memcpy(report + 10, &nan, sizeof nan);
    KF_EXPECT(!kf_sensor_decode_with(&f32, report, 22u, 1u, &s)); /* Non-finite: dropped. */

    KF_EXPECT(!kf_sensor_decode_with(&i16, report, 21u, 1u, &s)); /* Wrong length. */
    const kf_sensor_layout past_end = {22u, 12u, KF_SAMPLE_INT32, 1.0f};
    KF_EXPECT(!kf_sensor_decode_with(&past_end, report, 22u, 1u, &s));
    const kf_sensor_layout bad_type = {22u, 0u, 9u, 1.0f};
    KF_EXPECT(!kf_sensor_decode_with(&bad_type, report, 22u, 1u, &s));
    KF_EXPECT(!kf_sensor_decode_with(NULL, report, 22u, 1u, &s));
#if !KF_SENSOR_LAYOUT_MEASURED
    KF_EXPECT(!kf_sensor_decode_report(report, 22u, 1u, &s)); /* Never a guess. */
#endif
}

void test_sensor_falls_back_without_a_layout(void) {
    reset_all();
#if !KF_SENSOR_LAYOUT_MEASURED
    KF_EXPECT(!kf_sensor_probe());
    KF_EXPECT(kf_sensor_start() == KF_ERR_NOT_FOUND);
    KF_EXPECT(kf_sensor_state_now(0u) == KF_SENSOR_ABSENT);
    const kf_key_event down = key_down(10u * MS, true);
    kf_keytap_dispatch(&down); /* Plays at once on queue A. */
    KF_EXPECT(queued(KF_QUEUE_KEYTAP) == 2u);
    kf_sensor_stop();
#endif
    kf_sensor_reset();
}

void test_sensor_expired_keys_play_at_default(void) {
    reset_all();
    const kf_key_event down = key_down(500u * MS, true);
    KF_EXPECT(kf_sensor_push_pending(&down));
    kf_sensor_resolve_expired(539u * MS); /* 39 ms old: still waits for samples. */
    KF_EXPECT(queued(KF_QUEUE_SENSOR) == 0u);
    kf_sensor_resolve_expired(541u * MS); /* Over KF_PENDING_MAX_AGE_NS: plays now. */
    KF_EXPECT(queued(KF_QUEUE_SENSOR) == 2u);
    KF_EXPECT_NEAR(kf_sensor_last_velocity(), KF_DEFAULT_VELOCITY, 1e-6);
}

void test_sensor_reports_peaks_and_rolling_force(void) {
    reset_all();
    KF_EXPECT(kf_sensor_rolling_velocity() < 0.0f);
    const float spikes[3] = {0.02f, 0.05f, 0.30f};
    for (int k = 0; k < 3; ++k) {
        const uint64_t t = 500u + 100u * (uint64_t)k;
        const kf_key_event down = key_down(t * MS, true);
        KF_EXPECT(kf_sensor_push_pending(&down));
        feed(t - 70u, t + 10u, t + 2u, spikes[k]);
    }
    kf_sensor_peak peak;
    float sum = 0.0f;
    for (int k = 0; k < 3; ++k) {
        KF_EXPECT(kf_sensor_pop_peak(&peak));
        KF_EXPECT_NEAR(peak.raw_g, spikes[k], 1e-4);
        KF_EXPECT(peak.row == KF_ROW_HOME && peak.t_ns == (500u + 100u * (uint64_t)k) * MS);
        sum += peak.velocity;
    }
    KF_EXPECT(!kf_sensor_pop_peak(&peak));
    KF_EXPECT_NEAR(kf_sensor_rolling_velocity(), sum / 3.0f, 1e-5);
}

void test_sensor_takes_calibration_while_running(void) {
    reset_all();
    kf_sensor_set_state(KF_SENSOR_LIVE);
    kf_calibration hot = kf_calibration_default();
    hot.soft_g = 0.001f;
    hot.slam_g = 0.002f;
    hot.auto_adapt = false;
    KF_EXPECT(kf_sensor_set_calibration(&hot) == KF_OK); /* Queued, not applied yet. */
    KF_EXPECT(kf_sensor_context()->calibration.slam_g != 0.002f);
    kf_sensor_apply_pending_calibration(); /* As the sensor thread does per report. */
    KF_EXPECT(kf_sensor_context()->calibration.slam_g == 0.002f);
    for (int i = 0; i < 4; ++i) {
        KF_EXPECT(kf_sensor_set_calibration(&hot) == KF_OK);
    }
    KF_EXPECT(kf_sensor_set_calibration(&hot) == KF_ERR_LIMIT); /* The thread fell behind. */
    kf_sensor_apply_pending_calibration();
    hot.slam_g = hot.soft_g; /* Invalid: slam must exceed soft. */
    KF_EXPECT(kf_sensor_set_calibration(&hot) == KF_ERR_INVALID_ARGUMENT);
    kf_sensor_set_state(KF_SENSOR_STOPPED);
    kf_sensor_reset();
    const kf_calibration def = kf_calibration_default();
    KF_EXPECT(kf_sensor_set_calibration(&def) == KF_OK);
}

void test_sensor_measures_noise_floor(void) {
    reset_all();
    KF_EXPECT(kf_sensor_noise_floor_g() < 0.0f);
    kf_sensor_context()->noise_until_ns = 0u;
    /* Rest with repeated small bumps and one accidental large bump. */
    kf_sensor_begin_noise_measurement(0u);
    atomic_store(&kf_sensor_context()->noise_until_ns, 300u * MS);
    for (uint64_t t = 1u; t < 300u; ++t) {
        const float bump = t == 150u ? 0.2f : (t % 20u == 0u ? 0.004f : 0.0f);
        const kf_accel_sample s = {t * MS, 0.0f, 0.0f, 1.0f + bump};
        kf_sensor_process_sample(&s);
    }
    KF_EXPECT(kf_sensor_noise_floor_g() < 0.0f); /* Still measuring. */
    feed(300u, 300u, 0u, 0.0f);
    KF_EXPECT(kf_sensor_noise_floor_g() > 0.003f && kf_sensor_noise_floor_g() < 0.01f);
    kf_sensor_counters c;
    kf_sensor_get_counters(&c);
    KF_EXPECT_NEAR(c.sample_rate_hz, 1000.0, 1.0);
}

/* The driver's reporting settings. Only the plans and the argument checks run here: a valid
 * wake would write to the real driver, which a unit test must never do. */

static bool write_is(const kf_driver_write *w, const char *key, int32_t value) {
    return strcmp(w->key, key) == 0 && w->value == value;
}

void test_sensor_wake_plan_orders_writes(void) {
    kf_driver_write w[KF_DRIVER_MAX_WRITES];
    KF_EXPECT(kf_sensor_wake_plan(KF_SENSOR_WAKE_INTERVAL, 2500u, w) == 1u);
    KF_EXPECT(write_is(&w[0], "ReportInterval", 2500));
    KF_EXPECT(kf_sensor_wake_plan(KF_SENSOR_WAKE_REPORTING, 5000u, w) == 2u);
    KF_EXPECT(write_is(&w[0], "SensorPropertyReportingState", 1));
    KF_EXPECT(write_is(&w[1], "ReportInterval", 5000));
    KF_EXPECT(kf_sensor_wake_plan(KF_SENSOR_WAKE_POWER, KF_SENSOR_MIN_INTERVAL_US, w) == 3u);
    KF_EXPECT(write_is(&w[0], "SensorPropertyReportingState", 1));
    KF_EXPECT(write_is(&w[1], "SensorPropertyPowerState", 1));
    KF_EXPECT(write_is(&w[2], "ReportInterval", (int32_t)KF_SENSOR_MIN_INTERVAL_US));
    KF_EXPECT(kf_sensor_wake_plan(KF_SENSOR_WAKE_POWER, KF_SENSOR_MAX_INTERVAL_US, w) == 3u);
}

void test_sensor_wake_plan_rejects_bad_input(void) {
    kf_driver_write w[KF_DRIVER_MAX_WRITES];
    const union {
        kf_sensor_wake value;
        unsigned int raw;
    } invalid_wake = {.raw = 4u};
    KF_EXPECT(kf_sensor_wake_plan(KF_SENSOR_WAKE_NONE, 2500u, w) == 0u);
    KF_EXPECT(kf_sensor_wake_plan(invalid_wake.value, 2500u, w) == 0u);
    KF_EXPECT(kf_sensor_wake_plan(KF_SENSOR_WAKE_POWER, KF_SENSOR_MIN_INTERVAL_US - 1u, w) == 0u);
    KF_EXPECT(kf_sensor_wake_plan(KF_SENSOR_WAKE_POWER, KF_SENSOR_MAX_INTERVAL_US + 1u, w) == 0u);
    KF_EXPECT(kf_sensor_wake_plan(KF_SENSOR_WAKE_POWER, 0u, w) == 0u);
    KF_EXPECT(kf_sensor_wake_plan(KF_SENSOR_WAKE_POWER, 2500u, NULL) == 0u);
}

void test_sensor_restore_plan_puts_back(void) {
    kf_driver_write w[KF_DRIVER_MAX_WRITES];
    /* As measured on Mac17,3: interval 0, the two states not shown. */
    const kf_sensor_driver_state off = {0, -1, -1, 0};
    KF_EXPECT(kf_sensor_restore_plan(KF_SENSOR_WAKE_POWER, 3u, &off, w) == 3u);
    KF_EXPECT(write_is(&w[0], "SensorPropertyReportingState", 0));
    KF_EXPECT(write_is(&w[1], "SensorPropertyPowerState", 0));
    KF_EXPECT(write_is(&w[2], "ReportInterval", 0));
    /* Shown values come back as they were. */
    const kf_sensor_driver_state shown = {8000, 0, 2, 41};
    KF_EXPECT(kf_sensor_restore_plan(KF_SENSOR_WAKE_POWER, 3u, &shown, w) == 3u);
    KF_EXPECT(write_is(&w[1], "SensorPropertyPowerState", 2));
    KF_EXPECT(write_is(&w[2], "ReportInterval", 8000));
    /* Undoing a partial wake: only the writes that were made. */
    KF_EXPECT(kf_sensor_restore_plan(KF_SENSOR_WAKE_POWER, 1u, &off, w) == 1u);
    KF_EXPECT(write_is(&w[0], "SensorPropertyReportingState", 0));
    KF_EXPECT(kf_sensor_restore_plan(KF_SENSOR_WAKE_POWER, 0u, &off, w) == 0u);
    KF_EXPECT(kf_sensor_restore_plan(KF_SENSOR_WAKE_INTERVAL, 3u, &shown, w) == 1u);
    KF_EXPECT(write_is(&w[0], "ReportInterval", 8000));
    KF_EXPECT(kf_sensor_restore_plan(KF_SENSOR_WAKE_NONE, 3u, &off, w) == 0u);
    KF_EXPECT(kf_sensor_restore_plan(KF_SENSOR_WAKE_POWER, 3u, NULL, w) == 0u);
}

void test_sensor_driver_checks_before_touching_it(void) {
    /* Each of these returns before looking for the driver. */
    const union {
        kf_sensor_wake value;
        unsigned int raw;
    } invalid_wake = {.raw = 9u};
    KF_EXPECT(kf_sensor_driver_woken() == KF_SENSOR_WAKE_NONE);
    KF_EXPECT(kf_sensor_driver_wake(KF_SENSOR_WAKE_NONE, 0u) == KF_OK);
    KF_EXPECT(kf_sensor_driver_wake(KF_SENSOR_WAKE_POWER, 1u) == KF_ERR_INVALID_ARGUMENT);
    KF_EXPECT(kf_sensor_driver_wake(invalid_wake.value, 2500u) == KF_ERR_INVALID_ARGUMENT);
    KF_EXPECT(kf_sensor_driver_woken() == KF_SENSOR_WAKE_NONE);
    KF_EXPECT(kf_sensor_driver_restore() == KF_OK); /* Nothing was changed. */
    KF_EXPECT(kf_sensor_driver_read(NULL) == KF_ERR_INVALID_ARGUMENT);
}
