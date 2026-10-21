// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/**
 * @file kf_rt.h
 * @brief Real-time annotations and low-level helpers. Header-only, no dependencies.
 *
 * KF_RT_SAFE marks a function that runs on the audio thread, the sensor callback
 * or the keytap callback. Such functions must not allocate, free, lock, log,
 * send Objective-C messages, touch files or the network, or block. Code that is
 * only RT-safe lives in *_rt.c files, which `make check-rt` scans for banned
 * symbols (docs/ARCHITECTURE.md, "Real-time translation units").
 */
#ifndef KF_RT_H
#define KF_RT_H

#define KF_RT_SAFE

#if defined(__GNUC__) || defined(__clang__)
#define KF_LIKELY(x)    __builtin_expect(!!(x), 1)
#define KF_UNLIKELY(x)  __builtin_expect(!!(x), 0)
#define KF_MAYBE_UNUSED __attribute__((unused))
#else
#define KF_LIKELY(x)   (x)
#define KF_UNLIKELY(x) (x)
#define KF_MAYBE_UNUSED
#endif

/** Apple Silicon cache lines are 128 bytes; separating hot atomics avoids false sharing. */
#define KF_CACHELINE 128

#define KF_ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

#endif /* KF_RT_H */
