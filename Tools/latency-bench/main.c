// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/*
 * latency-bench: times the keystroke hot path (classify -> velocity -> queue)
 * and checks it against the budget in docs/PERFORMANCE.md. The audio thread's
 * drain is simulated untimed after each keystroke, as real typing is far slower
 * than the queue; concurrent behaviour is covered by the SPSC stress test under
 * ThreadSanitizer. A second pass times the render callback's work for one buffer with the
 * whole voice pool sounding, the worst case. A third times Shout Mode's decision in the active
 * tap's callback, with the sensor live and the rolling force high, also the worst case. Big
 * Enter is on throughout, so hard Enters queue the boom too. Exit status 1 when any p99
 * exceeds its budget or a request drops.
 *
 * Usage: latency-bench [--iterations N]
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <kf/kf.h>

#include "engine/kf_engine_internal.h"
#include "keymap/kf_vk.h"
#include "keytap/kf_keytap_internal.h"
#include "sensor/kf_sensor_internal.h"

#define KF_BENCH_DEFAULT_ITERATIONS 100000u
#define KF_BENCH_BUDGET_P99_NS      200000u /* docs/PERFORMANCE.md */
#define KF_BENCH_RENDER_RUNS        20000u
#define KF_BENCH_TAP_BUDGET_NS      50000u /* Spec: the tap callback returns within 50 us. */
#define KF_BENCH_RATE               48000.0
#define MS                          1000000ULL

static kf_accel_ring g_ring;

static int compare_u64(const void *a, const void *b) {
    const uint64_t x = *(const uint64_t *)a;
    const uint64_t y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

static void fill_ring(void) {
    kf_accel_ring_reset(&g_ring);
    for (uint64_t t = 0u; t < 1000u; ++t) {
        const float spike = (t % 50u == 2u) ? 0.01f * (float)(t % 20u + 1u) : 0.0f;
        const kf_accel_sample s = {t * MS, 0.01f, -0.02f, 1.0f + spike};
        kf_accel_ring_push(&g_ring, &s);
    }
}

static uint32_t parse_iterations(int argc, char **argv) {
    uint32_t iterations = KF_BENCH_DEFAULT_ITERATIONS;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--iterations") == 0 && i + 1 < argc) {
            const unsigned long value = strtoul(argv[++i], NULL, 10);
            iterations = value > 0u && value <= 100000000u ? (uint32_t)value : iterations;
        } else {
            fprintf(stderr, "usage: latency-bench [--iterations N]\n");
            exit(EXIT_FAILURE); // NOLINT(concurrency-mt-unsafe): single-threaded here.
        }
    }
    return iterations;
}

static uint64_t percentile(uint64_t *sorted, uint32_t count, uint32_t pct) {
    return sorted[(uint32_t)(((uint64_t)count * pct) / 100u)];
}

/* Render budget: 10% of one buffer period (docs/PERFORMANCE.md). */
static int bench_render(void) {
    static int16_t take[48000]; /* 1 s, so no voice ends during the run. */
    for (uint32_t i = 0u; i < 48000u; ++i) {
        take[i] = (int16_t)((int32_t)((i * 37u) % 2000u) - 1000);
    }
    kf_pack_pcm sounds[KF_TIER_COUNT];
    for (uint8_t t = 0u; t < KF_TIER_COUNT; ++t) {
        sounds[t] = (kf_pack_pcm){KF_GROUP_ALPHA, 0u, t, false, take, 48000u};
    }
    kf_pack *pack = kf_pack_from_pcm(sounds, KF_TIER_COUNT, KF_BENCH_RATE, NULL, 0u);
    if (pack == NULL) {
        fprintf(stderr, "latency-bench: cannot build the bench pack\n");
        return EXIT_FAILURE;
    }
    (void)kf_engine_init();
    kf_engine_set_pack(pack);
    kf_engine_render_reset(KF_BENCH_RATE);
    static float left[KF_IO_BUFFER_FRAMES];
    static float right[KF_IO_BUFFER_FRAMES];
    float *const channels[2] = {left, right};
    for (uint32_t v = 0u; v < KF_MAX_VOICES; ++v) {
        const kf_key_event down = kf_keymap_make_event((uint16_t)v, true, 0u, true);
        (void)kf_emit_keystroke(&down, 0.0f, KF_QUEUE_KEYTAP); /* One voice each. */
    }
    (void)kf_engine_render(channels, 2u, KF_IO_BUFFER_FRAMES, 0u);

    uint64_t *elapsed = malloc(KF_BENCH_RENDER_RUNS * sizeof *elapsed);
    if (elapsed == NULL) {
        kf_engine_set_pack(NULL);
        return EXIT_FAILURE;
    }
    for (uint32_t i = 0u; i < KF_BENCH_RENDER_RUNS; ++i) {
        if (kf_engine_render_active_voices() < KF_MAX_VOICES) {
            kf_engine_render_reset(KF_BENCH_RATE); /* Refill: keep the pool full. */
            for (uint32_t v = 0u; v < KF_MAX_VOICES; ++v) {
                const kf_key_event down = kf_keymap_make_event((uint16_t)v, true, 0u, true);
                (void)kf_emit_keystroke(&down, 0.0f, KF_QUEUE_KEYTAP);
            }
        }
        const uint64_t start = kf_time_now_ns();
        (void)kf_engine_render(channels, 2u, KF_IO_BUFFER_FRAMES, 0u);
        elapsed[i] = kf_time_now_ns() - start;
    }
    qsort(elapsed, KF_BENCH_RENDER_RUNS, sizeof *elapsed, compare_u64);
    const uint64_t budget =
        (uint64_t)((double)KF_IO_BUFFER_FRAMES / KF_BENCH_RATE * 1e9 * 0.10); /* 266 us */
    const uint64_t p99 = percentile(elapsed, KF_BENCH_RENDER_RUNS, 99u);
    printf("latency-bench: render %u frames with %u voices (%u buffers)\n", KF_IO_BUFFER_FRAMES,
           KF_MAX_VOICES, KF_BENCH_RENDER_RUNS);
    printf("  p50 %8llu ns\n  p99 %8llu ns   budget %llu ns\n  max %8llu ns\n",
           (unsigned long long)percentile(elapsed, KF_BENCH_RENDER_RUNS, 50u),
           (unsigned long long)p99, (unsigned long long)budget,
           (unsigned long long)elapsed[KF_BENCH_RENDER_RUNS - 1u]);
    free(elapsed);
    kf_engine_set_pack(NULL);
    return p99 <= budget ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* Shout Mode, as the active tap's callback runs it for each key-down. */
static int bench_shout(uint32_t iterations, uint64_t *latency) {
    kf_sensor_reset();
    kf_sensor_ctx *sensor = kf_sensor_context();
    kf_keytap_ctx *tap = kf_keytap_context();
    atomic_store(&sensor->rolling_velocity, 0.9f);
    atomic_store(&tap->external_keyboards, 0u);
    kf_sensor_set_state(KF_SENSOR_LIVE);
    kf_keytap_set_shout(true);
    uint64_t shouted = 0u;
    for (uint32_t i = 0u; i < iterations; ++i) {
        const uint16_t keycode = (uint16_t)(i % 0x30u); /* Letters and other keys. */
        const uint64_t start = kf_time_now_ns();
        atomic_store_explicit(&sensor->last_sample_ns, start, memory_order_relaxed);
        const uint64_t flags = kf_keytap_shout(tap, keycode, 0u, start);
        latency[i] = kf_time_now_ns() - start;
        shouted += flags != 0u ? 1u : 0u;
    }
    kf_keytap_set_shout(false);
    kf_sensor_reset();
    qsort(latency, iterations, sizeof *latency, compare_u64);
    const uint64_t p99 = latency[(uint32_t)(((uint64_t)iterations * 99u) / 100u)];
    printf("latency-bench: Shout Mode decision, sensor live (%u key-downs, %llu capitals)\n",
           iterations, (unsigned long long)shouted);
    printf("  p50 %8llu ns\n  p99 %8llu ns   budget %u ns\n  max %8llu ns\n",
           (unsigned long long)latency[iterations / 2u], (unsigned long long)p99,
           KF_BENCH_TAP_BUDGET_NS, (unsigned long long)latency[iterations - 1u]);
    return p99 <= KF_BENCH_TAP_BUDGET_NS && shouted > 0u ? EXIT_SUCCESS : EXIT_FAILURE;
}

int main(int argc, char **argv) {
    const uint32_t iterations = parse_iterations(argc, argv);
    kf_time_init();
    (void)kf_engine_init();
    kf_engine_set_big_enter(true);
    fill_ring();
    const kf_calibration cal = kf_calibration_default();

    uint64_t *latency = malloc((size_t)iterations * sizeof *latency);
    if (latency == NULL) {
        fprintf(stderr, "latency-bench: out of memory\n");
        return EXIT_FAILURE;
    }

    kf_voice_queue *queue = kf_engine_voice_queue(KF_QUEUE_SENSOR);
    kf_voice_req drained;
    for (uint32_t i = 0u; i < iterations; ++i) {
        const uint16_t keycode = (uint16_t)(i % 0x30u);
        const uint64_t t_key = (100u + (uint64_t)(i % 850u)) * MS;
        const uint64_t start = kf_time_now_ns();
        const kf_key_event event = kf_keymap_make_event(keycode, true, t_key, true);
        float velocity = kf_velocity_estimate(&g_ring, event.t_ns, event.row, &cal);
        if (velocity < 0.0f) {
            velocity = KF_DEFAULT_VELOCITY;
        }
        (void)kf_emit_keystroke(&event, velocity, KF_QUEUE_SENSOR);
        latency[i] = kf_time_now_ns() - start;
        while (kf_voice_queue_pop(queue, &drained)) { /* Untimed: stands in for the audio thread. */
        }
    }

    qsort(latency, iterations, sizeof *latency, compare_u64);
    const uint64_t p50 = latency[iterations / 2u];
    const uint64_t p99 = latency[(uint32_t)(((uint64_t)iterations * 99u) / 100u)];
    const uint64_t max = latency[iterations - 1u];

    kf_engine_counters counters;
    kf_engine_get_counters(&counters);
    printf("latency-bench: %u keystrokes (classify + velocity + queue)\n", iterations);
    printf("  p50 %8llu ns\n  p99 %8llu ns   budget %u ns\n  max %8llu ns\n",
           (unsigned long long)p50, (unsigned long long)p99, KF_BENCH_BUDGET_P99_NS,
           (unsigned long long)max);
    printf("  dropped requests: %llu\n", (unsigned long long)counters.requests_dropped);

    const int render = bench_render();
    const int shout = bench_shout(iterations, latency);
    free(latency);
    if (p99 > KF_BENCH_BUDGET_P99_NS || counters.requests_dropped != 0u || render != EXIT_SUCCESS ||
        shout != EXIT_SUCCESS) {
        printf("FAIL: p99 over budget or requests dropped\n");
        return EXIT_FAILURE;
    }
    printf("PASS\n");
    return EXIT_SUCCESS;
}
