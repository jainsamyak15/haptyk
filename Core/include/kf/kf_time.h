// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/**
 * @file kf_time.h
 * @brief One monotonic clock, in nanoseconds, shared by every thread.
 *
 * All timestamps in the core (key events, sensor samples, voice requests) use
 * this clock. Milestone 0 confirms the units of CGEventGetTimestamp and of HID
 * report timestamps so both can be converted onto it.
 */
#ifndef KF_TIME_H
#define KF_TIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Reads the timebase. Call once at startup, before any thread starts. */
void kf_time_init(void);

/** Current monotonic time in ns. RT-safe. */
uint64_t kf_time_now_ns(void);

/** Converts mach_absolute_time ticks to ns. RT-safe. */
uint64_t kf_time_ticks_to_ns(uint64_t ticks);

/** Exact, overflow-safe tick conversion for a given timebase. Pure. */
uint64_t kf_time_ticks_to_ns_with(uint64_t ticks, uint32_t numer, uint32_t denom);

#ifdef __cplusplus
}
#endif

#endif /* KF_TIME_H */
