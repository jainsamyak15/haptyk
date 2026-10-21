// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#include <kf/kf_time.h>

#if defined(__APPLE__)
#include <mach/mach_time.h>
#else
#include <time.h> /* Host-side unit tests only; the product is macOS-only. */
#endif

/* Written once by kf_time_init() before any thread starts; read-only afterwards. */
static uint32_t g_numer = 1u;
static uint32_t g_denom = 1u;

void kf_time_init(void) {
#if defined(__APPLE__)
    mach_timebase_info_data_t info;
    if (mach_timebase_info(&info) == KERN_SUCCESS && info.denom != 0u) {
        g_numer = info.numer;
        g_denom = info.denom;
    }
#endif
}

uint64_t kf_time_ticks_to_ns_with(uint64_t ticks, uint32_t numer, uint32_t denom) {
    if (denom == 0u) {
        return 0u;
    }
    /* Split so neither product overflows: exact for any realistic uptime. */
    return (ticks / denom) * numer + ((ticks % denom) * numer) / denom;
}

uint64_t kf_time_ticks_to_ns(uint64_t ticks) {
    return kf_time_ticks_to_ns_with(ticks, g_numer, g_denom);
}

uint64_t kf_time_now_ns(void) {
#if defined(__APPLE__)
    return kf_time_ticks_to_ns(mach_absolute_time());
#else
    struct timespec ts;
    (void)clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}
