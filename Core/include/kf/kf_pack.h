// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/**
 * @file kf_pack.h
 * @brief Decoded sound packs held in RAM.
 *
 * The Objective-C layer (KFPackLibrary) parses and validates pack.json and passes a flat
 * file list here; the core never parses JSON. The pack applies the fallback rules itself
 * (docs/SPEC.md, "Sound pack format") so a fallback shares the decoded audio of the sound
 * it falls back to instead of decoding a copy (ADR-0012):
 *
 *   - a missing tier uses the nearest tier of the same group, ties going to the softer;
 *   - a group with no key-down sounds uses alpha's (its release sounds, if any, stay its own);
 *   - a group with neither uses alpha's key-down and release sounds;
 *   - a missing release list means no release sound.
 *
 * Every pack needs at least one alpha key-down sound. While Big Enter (Labs) is on, the app
 * also passes the boom as one more file (group KF_SOUND_BOOM, key-down, tier 0), so it is
 * decoded at the device rate, swapped and freed with the pack. No fallback involves it.
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
    uint8_t group;    /**< kf_key_group, or KF_SOUND_BOOM. */
    uint8_t is_up;    /**< 0 key-down, 1 key-up. */
    uint8_t tier;     /**< kf_tier; ignored for key-up. */
    bool derived;     /**< Generated tier, not a real recording. */
    const char *path; /**< Absolute path. */
} kf_pack_file;

/** One decoded sound, already in memory: for packs built from generated audio and tests. */
typedef struct kf_pack_pcm {
    uint8_t group;
    uint8_t is_up;
    uint8_t tier;
    bool derived;
    const int16_t *samples; /**< Mono, at the pack's rate. Copied by kf_pack_from_pcm(). */
    uint32_t frames;
} kf_pack_pcm;

/** One playable take: 16-bit mono at the pack's sample rate. */
typedef struct kf_pack_sound {
    const int16_t *samples;
    uint32_t frames;
} kf_pack_sound;

/** Opaque: one contiguous block of decoded audio plus an index. */
typedef struct kf_pack kf_pack;

/**
 * Decodes every file to 16-bit mono at `device_rate` with ExtAudioFile, into one
 * contiguous block whose every page is written, so playback never page-faults or
 * resamples. Files must be WAV or CAF holding 16- or 24-bit PCM or ALAC, mono, 44.1 or
 * 48 kHz, at most KF_PACK_MAX_FILE_MS long. Enforces KF_PACK_MAX_FILES and
 * KF_PACK_MAX_DECODED_BYTES. Returns NULL and a readable message in `err` on failure.
 * Background thread only; never on a real-time thread.
 */
kf_pack *kf_pack_load(const kf_pack_file *files, size_t count, double device_rate, char *err,
                      size_t errlen);

/** Builds a pack from decoded audio at `rate`, with the same rules and limits. */
kf_pack *kf_pack_from_pcm(const kf_pack_pcm *sounds, size_t count, double rate, char *err,
                          size_t errlen);

/** Frees a pack. NULL is allowed. Never while the engine may still play it. */
void kf_pack_free(kf_pack *pack);

/** Bytes of decoded audio held. */
size_t kf_pack_decoded_bytes(const kf_pack *pack);

/** Sample rate the pack was decoded at. */
double kf_pack_sample_rate(const kf_pack *pack);

/**
 * The takes for one key: its key-down sounds for `tier`, or its release sounds when
 * `is_up`. Fallbacks are already applied. Returns the number of takes (0: play nothing)
 * and points *out at them. RT-safe.
 */
uint32_t kf_pack_sounds(const kf_pack *pack, uint8_t group, bool is_up, uint8_t tier,
                        const kf_pack_sound **out);

/** True when the takes for a key-down tier were generated rather than recorded. */
bool kf_pack_is_derived(const kf_pack *pack, uint8_t group, uint8_t tier);

#ifdef __cplusplus
}
#endif

#endif /* KF_PACK_H */
