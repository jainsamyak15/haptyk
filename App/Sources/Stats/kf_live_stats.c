// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#include "Stats/kf_live_stats.h"

#define NS_PER_S   1000000000ULL
#define NS_PER_MIN (60ULL * NS_PER_S)

/* One writer per slot (the keytap thread), so load-then-store is exact; readers may see a
 * slot mid-reset, which only ever under-counts that one second or minute. */
static void bump_slot(_Atomic(uint64_t) *ids, _Atomic(uint32_t) *counts, uint32_t size,
                      uint64_t id) {
    const uint32_t slot = (uint32_t)(id % size);
    if (atomic_load_explicit(&ids[slot], memory_order_relaxed) != id) {
        atomic_store_explicit(&counts[slot], 0u, memory_order_relaxed);
        atomic_store_explicit(&ids[slot], id, memory_order_release);
    }
    atomic_store_explicit(&counts[slot],
                          atomic_load_explicit(&counts[slot], memory_order_relaxed) + 1u,
                          memory_order_relaxed);
}

void kf_live_stats_note_key(kf_live_stats *stats, uint64_t t_ns) {
    atomic_store_explicit(&stats->keys,
                          atomic_load_explicit(&stats->keys, memory_order_relaxed) + 1u,
                          memory_order_relaxed);
    /* +1: slot ids of zero would match the zeroed initial state. */
    bump_slot(stats->second_id, stats->second_count, KF_LIVE_SECONDS, t_ns / NS_PER_S + 1u);
    bump_slot(stats->minute_id, stats->minute_count, KF_LIVE_MINUTES, t_ns / NS_PER_MIN + 1u);
}

uint32_t kf_live_stats_last_minute(const kf_live_stats *stats, uint64_t now_ns) {
    const uint64_t now = now_ns / NS_PER_S + 1u;
    uint32_t total = 0u;
    for (uint32_t i = 0u; i < KF_LIVE_SECONDS; ++i) {
        const uint64_t id = atomic_load_explicit(&stats->second_id[i], memory_order_acquire);
        if (id != 0u && id <= now && now - id < 60u) {
            total += atomic_load_explicit(&stats->second_count[i], memory_order_relaxed);
        }
    }
    return total;
}

void kf_live_stats_minutes(const kf_live_stats *stats, uint64_t from_minute, uint64_t until_minute,
                           void (*visit)(uint32_t count, void *ctx), void *ctx) {
    for (uint32_t i = 0u; i < KF_LIVE_MINUTES; ++i) {
        const uint64_t id = atomic_load_explicit(&stats->minute_id[i], memory_order_acquire);
        if (id == 0u) {
            continue;
        }
        const uint64_t minute = id - 1u;
        const uint32_t count = atomic_load_explicit(&stats->minute_count[i], memory_order_relaxed);
        if (minute > from_minute && minute < until_minute && count > 0u) {
            visit(count, ctx);
        }
    }
}
