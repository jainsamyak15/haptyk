// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/**
 * @file kf_spsc.h
 * @brief Bounded, wait-free single-producer/single-consumer queue generator.
 *
 * KF_SPSC_DEFINE(name, type, capacity) generates:
 *
 *     void     name_reset(name *q);                // before use; not thread-safe
 *     bool     name_push(name *q, const type *in); // producer only; false when full
 *     bool     name_pop(name *q, type *out);       // consumer only; false when empty
 *     bool     name_peek(name *q, type *out);      // consumer only; copies the oldest item
 *     uint32_t name_size_approx(const name *q);    // any thread; may be stale
 *
 * Capacity must be a power of two. Every operation is wait-free and RT-safe.
 * The producer and consumer indices sit on separate cache lines, and each side
 * caches the other's index, so the shared lines are touched only when needed.
 * Instances carry 128-byte alignment: declare them static or use aligned_alloc.
 */
#ifndef KF_SPSC_H
#define KF_SPSC_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#include "internal/kf_rt.h"

_Static_assert(ATOMIC_INT_LOCK_FREE == 2, "SPSC queues need lock-free 32-bit atomics");

// NOLINTBEGIN(bugprone-macro-parentheses): T is a type name and cannot be parenthesised.
#define KF_SPSC_DEFINE(NAME, T, CAPACITY)                                            \
    _Static_assert((CAPACITY) >= 2u && ((CAPACITY) & ((CAPACITY) - 1u)) == 0u,       \
                   #NAME ": capacity must be a power of two");                       \
                                                                                     \
    typedef struct NAME {                                                            \
        _Alignas(KF_CACHELINE) _Atomic(uint32_t) head; /* written by the producer */ \
        uint32_t cached_tail;                          /* producer's copy of tail */ \
        _Alignas(KF_CACHELINE) _Atomic(uint32_t) tail; /* written by the consumer */ \
        uint32_t cached_head;                          /* consumer's copy of head */ \
        _Alignas(KF_CACHELINE) T slots[CAPACITY];                                    \
    } NAME;                                                                          \
                                                                                     \
    KF_MAYBE_UNUSED static inline void NAME##_reset(NAME *q) {                       \
        atomic_store_explicit(&q->head, 0u, memory_order_relaxed);                   \
        atomic_store_explicit(&q->tail, 0u, memory_order_relaxed);                   \
        q->cached_tail = 0u;                                                         \
        q->cached_head = 0u;                                                         \
    }                                                                                \
                                                                                     \
    KF_MAYBE_UNUSED static inline bool NAME##_push(NAME *q, const T *item) {         \
        const uint32_t head = atomic_load_explicit(&q->head, memory_order_relaxed);  \
        if (head - q->cached_tail == (uint32_t)(CAPACITY)) {                         \
            q->cached_tail = atomic_load_explicit(&q->tail, memory_order_acquire);   \
            if (head - q->cached_tail == (uint32_t)(CAPACITY)) {                     \
                return false;                                                        \
            }                                                                        \
        }                                                                            \
        q->slots[head & ((uint32_t)(CAPACITY) - 1u)] = *item;                        \
        atomic_store_explicit(&q->head, head + 1u, memory_order_release);            \
        return true;                                                                 \
    }                                                                                \
                                                                                     \
    KF_MAYBE_UNUSED static inline bool NAME##_peek(NAME *q, T *out) {                \
        const uint32_t tail = atomic_load_explicit(&q->tail, memory_order_relaxed);  \
        if (tail == q->cached_head) {                                                \
            q->cached_head = atomic_load_explicit(&q->head, memory_order_acquire);   \
            if (tail == q->cached_head) {                                            \
                return false;                                                        \
            }                                                                        \
        }                                                                            \
        *out = q->slots[tail & ((uint32_t)(CAPACITY) - 1u)];                         \
        return true;                                                                 \
    }                                                                                \
                                                                                     \
    KF_MAYBE_UNUSED static inline bool NAME##_pop(NAME *q, T *out) {                 \
        if (!NAME##_peek(q, out)) {                                                  \
            return false;                                                            \
        }                                                                            \
        const uint32_t tail = atomic_load_explicit(&q->tail, memory_order_relaxed);  \
        atomic_store_explicit(&q->tail, tail + 1u, memory_order_release);            \
        return true;                                                                 \
    }                                                                                \
                                                                                     \
    KF_MAYBE_UNUSED static inline uint32_t NAME##_size_approx(const NAME *q) {       \
        const uint32_t head = atomic_load_explicit(&q->head, memory_order_acquire);  \
        const uint32_t tail = atomic_load_explicit(&q->tail, memory_order_acquire);  \
        return head - tail;                                                          \
    }
// NOLINTEND(bugprone-macro-parentheses)

#endif /* KF_SPSC_H */
