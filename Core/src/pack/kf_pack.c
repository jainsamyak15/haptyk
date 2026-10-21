// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/* Pack index, fallback rules and limits. Portable; decoding lives in kf_pack_decode.c. */

#include <kf/kf_pack.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <kf/kf_config.h>
#include <kf/kf_types.h>

#include "internal/kf_rt.h"
#include "pack/kf_pack_internal.h"

/* Sample-rate conversion may lengthen a file by a few frames; accept that much. */
#define KF_PACK_RESAMPLE_SLACK_FRAMES 64u
#define KF_PACK_MIN_RATE              8000.0
#define KF_PACK_MAX_RATE              384000.0

typedef struct kf_pack_range {
    uint16_t first;
    uint16_t count;
} kf_pack_range;

struct kf_pack {
    double rate;
    size_t decoded_bytes;
    int16_t *block; /* Every sample of the pack, one allocation. */
    kf_pack_range down[KF_GROUP_COUNT][KF_TIER_COUNT];
    kf_pack_range up[KF_GROUP_COUNT];
    bool derived[KF_GROUP_COUNT][KF_TIER_COUNT];
    uint32_t sound_count;
    kf_pack_sound sounds[]; /* Sorted by group, then tier, then release. */
};

KF_STATIC_ASSERT(KF_PACK_MAX_FILES <= UINT16_MAX, "pack ranges hold 16-bit indices");

kf_pack *kf_pack_fail(char *err, size_t errlen, const char *format, ...) {
    if (err != NULL && errlen > 0u) {
        va_list args;
        va_start(args, format);
        (void)vsnprintf(err, errlen, format, args);
        va_end(args);
    }
    return NULL;
}

bool kf_pack_entry_is_valid(uint8_t group, uint8_t is_up, uint8_t tier) {
    return group < KF_GROUP_COUNT && is_up <= 1u && (is_up != 0u || tier < KF_TIER_COUNT);
}

uint32_t kf_pack_max_frames(double rate) {
    return (uint32_t)(rate * (double)KF_PACK_MAX_FILE_MS / 1000.0) + KF_PACK_RESAMPLE_SLACK_FRAMES;
}

static uint32_t sort_key(const kf_pack_entry *e) {
    return (uint32_t)e->group * (KF_TIER_COUNT + 1u) +
           (e->is_up != 0u ? (uint32_t)KF_TIER_COUNT : (uint32_t)e->tier);
}

static void sort_entries(kf_pack_entry *entries, size_t count) {
    /* Stable insertion sort: at most KF_PACK_MAX_FILES entries, once per pack load. */
    for (size_t i = 1u; i < count; ++i) {
        const kf_pack_entry item = entries[i];
        const uint32_t key = sort_key(&item);
        size_t j = i;
        while (j > 0u && sort_key(&entries[j - 1u]) > key) {
            entries[j] = entries[j - 1u];
            j--;
        }
        entries[j] = item;
    }
}

static bool has_down(const kf_pack *p, uint8_t group) {
    for (int t = 0; t < (int)KF_TIER_COUNT; ++t) {
        if (p->down[group][t].count > 0u) {
            return true;
        }
    }
    return false;
}

/* A missing tier takes the nearest recorded one; at equal distance the softer wins. */
static void fill_missing_tiers(kf_pack *p, uint8_t group) {
    kf_pack_range resolved[KF_TIER_COUNT];
    bool derived[KF_TIER_COUNT];
    for (int t = 0; t < (int)KF_TIER_COUNT; ++t) {
        resolved[t] = p->down[group][t];
        derived[t] = p->derived[group][t];
        for (int d = 1; resolved[t].count == 0u && d < (int)KF_TIER_COUNT; ++d) {
            const int candidates[2] = {t - d, t + d}; /* Softer first. */
            for (int c = 0; c < 2; ++c) {
                const int s = candidates[c];
                if (s >= 0 && s < (int)KF_TIER_COUNT && p->down[group][s].count > 0u) {
                    resolved[t] = p->down[group][s];
                    derived[t] = p->derived[group][s];
                    break;
                }
            }
        }
    }
    memcpy(p->down[group], resolved, sizeof resolved);
    memcpy(p->derived[group], derived, sizeof derived);
}

kf_pack *kf_pack_build(int16_t *block, size_t total_frames, kf_pack_entry *entries, size_t count,
                       double rate, char *err, size_t errlen) {
    if (count == 0u || count > KF_PACK_MAX_FILES) {
        free(block);
        return kf_pack_fail(err, errlen, "a pack holds 1 to %u sounds, not %zu",
                            (unsigned)KF_PACK_MAX_FILES, count);
    }
    kf_pack *p = calloc(1u, sizeof *p + count * sizeof p->sounds[0]);
    if (p == NULL) {
        free(block);
        return kf_pack_fail(err, errlen, "out of memory");
    }
    sort_entries(entries, count);
    p->rate = rate;
    p->block = block;
    p->decoded_bytes = total_frames * sizeof(int16_t);
    p->sound_count = (uint32_t)count;
    for (size_t i = 0u; i < count; ++i) {
        const kf_pack_entry *e = &entries[i];
        p->sounds[i].samples = block + e->offset;
        p->sounds[i].frames = e->frames;
        kf_pack_range *range = e->is_up != 0u ? &p->up[e->group] : &p->down[e->group][e->tier];
        if (range->count == 0u) {
            range->first = (uint16_t)i;
        }
        range->count++;
        if (e->is_up == 0u && e->derived) {
            p->derived[e->group][e->tier] = true;
        }
    }

    if (!has_down(p, KF_GROUP_ALPHA)) {
        kf_pack_free(p);
        return kf_pack_fail(err, errlen, "a pack needs at least one alpha key-down sound");
    }
    fill_missing_tiers(p, KF_GROUP_ALPHA);
    for (uint8_t g = 0u; g < KF_GROUP_COUNT; ++g) {
        if (g == KF_GROUP_ALPHA) {
            continue;
        }
        if (has_down(p, g)) {
            fill_missing_tiers(p, g);
            continue;
        }
        /* A group without key-down sounds plays alpha's; with no sounds at all, also its
         * release sounds. */
        const bool empty = p->up[g].count == 0u;
        memcpy(p->down[g], p->down[KF_GROUP_ALPHA], sizeof p->down[g]);
        memcpy(p->derived[g], p->derived[KF_GROUP_ALPHA], sizeof p->derived[g]);
        if (empty) {
            p->up[g] = p->up[KF_GROUP_ALPHA];
        }
    }
    return p;
}

kf_pack *kf_pack_from_pcm(const kf_pack_pcm *sounds, size_t count, double rate, char *err,
                          size_t errlen) {
    if (sounds == NULL || count == 0u) {
        return kf_pack_fail(err, errlen, "no sounds given");
    }
    if (count > KF_PACK_MAX_FILES) {
        return kf_pack_fail(err, errlen, "%zu sounds; a pack holds at most %u", count,
                            (unsigned)KF_PACK_MAX_FILES);
    }
    if (!(rate >= KF_PACK_MIN_RATE && rate <= KF_PACK_MAX_RATE)) {
        return kf_pack_fail(err, errlen, "unsupported sample rate %.0f Hz", rate);
    }
    const uint32_t max_frames = kf_pack_max_frames(rate);
    size_t total = 0u;
    for (size_t i = 0u; i < count; ++i) {
        const kf_pack_pcm *s = &sounds[i];
        if (!kf_pack_entry_is_valid(s->group, s->is_up, s->tier) || s->samples == NULL ||
            s->frames == 0u) {
            return kf_pack_fail(err, errlen, "sound %zu: invalid group, tier or samples", i);
        }
        if (s->frames > max_frames) {
            return kf_pack_fail(err, errlen, "sound %zu: longer than %u ms", i,
                                (unsigned)KF_PACK_MAX_FILE_MS);
        }
        total += s->frames;
    }
    if (total * sizeof(int16_t) > KF_PACK_MAX_DECODED_BYTES) {
        return kf_pack_fail(err, errlen, "%zu KB of audio; a pack holds at most %u KB",
                            total * sizeof(int16_t) / 1024u,
                            (unsigned)(KF_PACK_MAX_DECODED_BYTES / 1024u));
    }

    int16_t *block = malloc(total * sizeof *block);
    kf_pack_entry *entries = malloc(count * sizeof *entries);
    if (block == NULL || entries == NULL) {
        free(block);
        free(entries);
        return kf_pack_fail(err, errlen, "out of memory");
    }
    size_t offset = 0u;
    for (size_t i = 0u; i < count; ++i) {
        const kf_pack_pcm *s = &sounds[i];
        memcpy(block + offset, s->samples, (size_t)s->frames * sizeof *block);
        entries[i] = (kf_pack_entry){s->group, s->is_up, s->tier, s->derived, offset, s->frames};
        offset += s->frames;
    }
    kf_pack *pack = kf_pack_build(block, total, entries, count, rate, err, errlen);
    free(entries);
    return pack;
}

void kf_pack_free(kf_pack *pack) {
    if (pack != NULL) {
        free(pack->block);
        free(pack);
    }
}

size_t kf_pack_decoded_bytes(const kf_pack *pack) {
    return pack != NULL ? pack->decoded_bytes : 0u;
}

double kf_pack_sample_rate(const kf_pack *pack) {
    return pack != NULL ? pack->rate : 0.0;
}

KF_RT_SAFE uint32_t kf_pack_sounds(const kf_pack *pack, uint8_t group, bool is_up, uint8_t tier,
                                   const kf_pack_sound **out) {
    if (pack == NULL || group >= KF_GROUP_COUNT || (!is_up && tier >= KF_TIER_COUNT)) {
        return 0u;
    }
    const kf_pack_range r = is_up ? pack->up[group] : pack->down[group][tier];
    *out = &pack->sounds[r.first];
    return r.count;
}

bool kf_pack_is_derived(const kf_pack *pack, uint8_t group, uint8_t tier) {
    return pack != NULL && group < KF_GROUP_COUNT && tier < KF_TIER_COUNT &&
           pack->derived[group][tier];
}
