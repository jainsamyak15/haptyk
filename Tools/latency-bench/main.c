// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/*
 * latency-bench: times the keystroke hot path (classify -> velocity -> queue)
 * and checks it against the budget in docs/PERFORMANCE.md. The audio thread's
 * drain is simulated untimed after each keystroke, as real typing is far slower
 * than the queue; concurrent behaviour is covered by the SPSC stress test under
 * ThreadSanitizer. Exit status 1 when p99 exceeds the budget or a request drops.
 *
 * Usage: latency-bench [--iterations N]
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <kf/kf.h>

#include "engine/kf_engine_internal.h"

#define KF_BENCH_DEFAULT_ITERATIONS 100000u
#define KF_BENCH_BUDGET_P99_NS      200000u /* docs/PERFORMANCE.md */
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

int main(int argc, char **argv) {
    const uint32_t iterations = parse_iterations(argc, argv);
    kf_time_init();
    (void)kf_engine_init();
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
    free(latency);

    kf_engine_counters counters;
    kf_engine_get_counters(&counters);
    printf("latency-bench: %u keystrokes (classify + velocity + queue)\n", iterations);
    printf("  p50 %8llu ns\n  p99 %8llu ns   budget %u ns\n  max %8llu ns\n",
           (unsigned long long)p50, (unsigned long long)p99, KF_BENCH_BUDGET_P99_NS,
           (unsigned long long)max);
    printf("  dropped requests: %llu\n", (unsigned long long)counters.requests_dropped);

    if (p99 > KF_BENCH_BUDGET_P99_NS || counters.requests_dropped != 0u) {
        printf("FAIL: p99 over budget or requests dropped\n");
        return EXIT_FAILURE;
    }
    printf("PASS\n");
    return EXIT_SUCCESS;
}
