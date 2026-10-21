// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/**
 * @file kf_pack_internal.h
 * @brief Pack module internals shared by the portable builder and the decoder.
 */
#ifndef KF_PACK_INTERNAL_H
#define KF_PACK_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <kf/kf_pack.h>

/** One sound inside the pack's sample block. */
typedef struct kf_pack_entry {
    uint8_t group;
    uint8_t is_up;
    uint8_t tier;
    bool derived;
    size_t offset;   /**< First sample, as an index into the block. */
    uint32_t frames; /**< Sample count. */
} kf_pack_entry;

/** printf-style message into err; safe with NULL or zero length. Always returns NULL. */
kf_pack *kf_pack_fail(char *err, size_t errlen, const char *format, ...)
    __attribute__((format(printf, 3, 4)));

/** Checks one entry's group and tier. */
bool kf_pack_entry_is_valid(uint8_t group, uint8_t is_up, uint8_t tier);

/** Longest sound accepted at `rate`, in frames, with a little room for resampling. */
uint32_t kf_pack_max_frames(double rate);

/**
 * Indexes `block` (total_frames samples, from malloc) and applies the fallback rules.
 * Takes ownership of `block` whether or not it succeeds. `entries` is sorted in place.
 */
kf_pack *kf_pack_build(int16_t *block, size_t total_frames, kf_pack_entry *entries, size_t count,
                       double rate, char *err, size_t errlen);

#endif /* KF_PACK_INTERNAL_H */
