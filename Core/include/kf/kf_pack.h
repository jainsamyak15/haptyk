// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/**
 * @file kf_pack.h
 * @brief Decoded sound packs held in RAM.
 *
 * The Objective-C layer (KFPackLibrary) parses and validates pack.json, applies
 * the fallback rules and passes a flat file list here. The core never parses JSON.
 */
#ifndef KF_PACK_H
#define KF_PACK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** One audio file of a pack. */
typedef struct kf_pack_file {
    uint8_t group;    /**< kf_key_group. */
    uint8_t is_up;    /**< 0 key-down, 1 key-up. */
    uint8_t tier;     /**< kf_tier; ignored for key-up. */
    bool derived;     /**< Generated tier, not a real recording. */
    const char *path; /**< Absolute path. */
} kf_pack_file;

/** Opaque: decoded 16-bit mono buffers plus an index. */
typedef struct kf_pack kf_pack;

/**
 * Decodes every file to 16-bit mono at `device_rate` and pre-touches the memory
 * so playback never page-faults. Returns NULL and writes a message to `err` on
 * failure. Background thread only; never on a real-time thread.
 */
kf_pack *kf_pack_load(const kf_pack_file *files, size_t count, double device_rate, char *err,
                      size_t errlen);

/** Frees a pack. NULL is allowed. */
void kf_pack_free(kf_pack *pack);

/** Bytes of decoded audio held. */
size_t kf_pack_decoded_bytes(const kf_pack *pack);

#ifdef __cplusplus
}
#endif

#endif /* KF_PACK_H */
