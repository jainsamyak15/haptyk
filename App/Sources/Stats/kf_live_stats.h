// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/**
 * @file kf_live_stats.h
 * @brief Keystroke counters updated on the keytap thread with atomics only, and read by the
 *        main thread (spec: "Updated with atomics on the hot path"). Counts only: never which
 *        key. Two rings of time slots give taps in the last minute and per-minute totals
 *        without any timer.
 */
#ifndef KF_LIVE_STATS_H
#define KF_LIVE_STATS_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KF_LIVE_SECONDS 64u /* > 60: the last full minute is always covered. */
#define KF_LIVE_MINUTES 64u /* > 5 minutes between flushes, with room to spare. */

typedef struct kf_live_stats {
    _Atomic(uint64_t) keys; /* Key-downs since launch. */
    _Atomic(uint64_t) second_id[KF_LIVE_SECONDS];
    _Atomic(uint32_t) second_count[KF_LIVE_SECONDS];
    _Atomic(uint64_t) minute_id[KF_LIVE_MINUTES];
    _Atomic(uint32_t) minute_count[KF_LIVE_MINUTES];
} kf_live_stats;

/** One key-down at `t_ns` (kf_time clock). Keytap thread only; RT-safe. */
void kf_live_stats_note_key(kf_live_stats *stats, uint64_t t_ns);

/** Key-downs in the 60 s before `now_ns`. Any thread. */
uint32_t kf_live_stats_last_minute(const kf_live_stats *stats, uint64_t now_ns);

/** Per-minute totals for complete minutes in (from_minute, until_minute): calls `visit` with
 * each non-empty minute's count. Minutes are t_ns / 60 s. Any thread. */
void kf_live_stats_minutes(const kf_live_stats *stats, uint64_t from_minute, uint64_t until_minute,
                           void (*visit)(uint32_t count, void *ctx), void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* KF_LIVE_STATS_H */
