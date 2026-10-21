// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "internal/kf_spsc.h"
#include "kf_test.h"

KF_SPSC_DEFINE(small_queue, uint32_t, 8u)
KF_SPSC_DEFINE(stress_queue, uint64_t, 1024u)

void test_spsc_push_pop_order(void) {
    static small_queue q;
    small_queue_reset(&q);
    for (uint32_t i = 0u; i < 5u; ++i) {
        KF_EXPECT(small_queue_push(&q, &i));
    }
    KF_EXPECT(small_queue_size_approx(&q) == 5u);
    for (uint32_t i = 0u; i < 5u; ++i) {
        uint32_t out = 99u;
        KF_EXPECT(small_queue_pop(&q, &out));
        KF_EXPECT(out == i);
    }
}

void test_spsc_full_and_empty(void) {
    static small_queue q;
    small_queue_reset(&q);
    uint32_t out = 0u;
    KF_EXPECT(!small_queue_pop(&q, &out));
    for (uint32_t i = 0u; i < 8u; ++i) {
        KF_EXPECT(small_queue_push(&q, &i));
    }
    const uint32_t extra = 8u;
    KF_EXPECT(!small_queue_push(&q, &extra));
    KF_EXPECT(small_queue_pop(&q, &out) && out == 0u);
    KF_EXPECT(small_queue_push(&q, &extra));
}

void test_spsc_wraparound(void) {
    static small_queue q;
    small_queue_reset(&q);
    /* Start near UINT32_MAX so the indices wrap during the test. */
    atomic_store(&q.head, UINT32_MAX - 3u);
    atomic_store(&q.tail, UINT32_MAX - 3u);
    q.cached_head = UINT32_MAX - 3u;
    q.cached_tail = UINT32_MAX - 3u;
    for (uint32_t round = 0u; round < 4u; ++round) {
        for (uint32_t i = 0u; i < 6u; ++i) {
            const uint32_t value = round * 100u + i;
            KF_EXPECT(small_queue_push(&q, &value));
        }
        for (uint32_t i = 0u; i < 6u; ++i) {
            uint32_t out = 0u;
            KF_EXPECT(small_queue_pop(&q, &out) && out == round * 100u + i);
        }
    }
}

void test_spsc_peek_does_not_consume(void) {
    static small_queue q;
    small_queue_reset(&q);
    const uint32_t value = 7u;
    KF_EXPECT(small_queue_push(&q, &value));
    uint32_t out = 0u;
    KF_EXPECT(small_queue_peek(&q, &out) && out == 7u);
    KF_EXPECT(small_queue_peek(&q, &out) && out == 7u);
    KF_EXPECT(small_queue_pop(&q, &out) && out == 7u);
    KF_EXPECT(!small_queue_peek(&q, &out));
}

typedef struct stress_args {
    stress_queue *queue;
    uint64_t count;
} stress_args;

static void *stress_producer(void *arg) {
    const stress_args *args = arg;
    for (uint64_t i = 0u; i < args->count;) {
        if (stress_queue_push(args->queue, &i)) {
            ++i;
        }
    }
    return NULL;
}

void test_spsc_threaded_stress(void) {
    static stress_queue q;
    stress_queue_reset(&q);
    uint64_t count = 10000000u; /* Spec: 10 million items; KF_STRESS_ITEMS overrides. */
    const char *env = getenv("KF_STRESS_ITEMS"); // NOLINT(concurrency-mt-unsafe): before threads.
    if (env != NULL) {
        count = strtoull(env, NULL, 10);
    }
    stress_args args = {&q, count};
    pthread_t producer;
    KF_EXPECT(pthread_create(&producer, NULL, stress_producer, &args) == 0);

    uint64_t received = 0u;
    bool in_order = true;
    while (received < count) {
        uint64_t value = 0u;
        if (stress_queue_pop(&q, &value)) {
            in_order = in_order && value == received;
            received++;
        }
    }
    KF_EXPECT(pthread_join(producer, NULL) == 0);
    KF_EXPECT(in_order);
    KF_EXPECT(received == count);
}
