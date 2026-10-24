// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <kf/kf_config.h>
#include <kf/kf_pack.h>
#include <kf/kf_types.h>

#include "kf_test.h"

#define RATE 48000.0

static int16_t g_tone[4800];

static kf_pack_pcm sound(uint8_t group, bool is_up, uint8_t tier, uint32_t frames) {
    const kf_pack_pcm s = {.group = group,
                           .is_up = is_up ? 1u : 0u,
                           .tier = tier,
                           .derived = false,
                           .samples = g_tone,
                           .frames = frames};
    return s;
}

static const int16_t *first_sample(const kf_pack *p, uint8_t group, bool is_up, uint8_t tier,
                                   uint32_t *count) {
    const kf_pack_sound *takes = NULL;
    *count = kf_pack_sounds(p, group, is_up, tier, &takes);
    return *count > 0u ? takes[0].samples : NULL;
}

void test_pack_fallback_rules(void) {
    /* Distinct lengths identify which sound a slot resolved to. */
    const kf_pack_pcm sounds[] = {
        sound(KF_GROUP_ALPHA, false, KF_TIER_SOFT, 100u),
        sound(KF_GROUP_ALPHA, false, KF_TIER_HARD, 300u),
        sound(KF_GROUP_ALPHA, true, 0u, 50u),
        sound(KF_GROUP_SPACE, false, KF_TIER_SLAM, 400u),
        sound(KF_GROUP_MODIFIER, true, 0u, 60u),
        sound(KF_GROUP_ALPHA, false, KF_TIER_SOFT, 101u), /* Second soft take, out of order. */
    };
    char err[128] = "";
    kf_pack *p = kf_pack_from_pcm(sounds, sizeof sounds / sizeof sounds[0], RATE, err, sizeof err);
    KF_EXPECT(p != NULL);
    if (p == NULL) {
        return;
    }
    const kf_pack_sound *takes = NULL;
    KF_EXPECT(kf_pack_sounds(p, KF_GROUP_ALPHA, false, KF_TIER_SOFT, &takes) == 2u);
    KF_EXPECT(takes[0].frames == 100u && takes[1].frames == 101u); /* Stable order. */
    /* Medium is equally far from soft and hard: the softer wins. */
    KF_EXPECT(kf_pack_sounds(p, KF_GROUP_ALPHA, false, KF_TIER_MEDIUM, &takes) == 2u &&
              takes[0].frames == 100u);
    KF_EXPECT(kf_pack_sounds(p, KF_GROUP_ALPHA, false, KF_TIER_SLAM, &takes) == 1u &&
              takes[0].frames == 300u);
    /* Space has only slam: every tier uses it, and it has no release sound of its own. */
    for (uint8_t t = 0u; t < (uint8_t)KF_TIER_COUNT; ++t) {
        KF_EXPECT(kf_pack_sounds(p, KF_GROUP_SPACE, false, t, &takes) == 1u &&
                  takes[0].frames == 400u);
    }
    uint32_t count = 0u;
    KF_EXPECT(first_sample(p, KF_GROUP_SPACE, true, 0u, &count) == NULL && count == 0u);
    /* Enter is missing entirely: alpha's key-down and release sounds. */
    KF_EXPECT(kf_pack_sounds(p, KF_GROUP_ENTER, false, KF_TIER_HARD, &takes) == 1u &&
              takes[0].frames == 300u);
    KF_EXPECT(kf_pack_sounds(p, KF_GROUP_ENTER, true, 0u, &takes) == 1u && takes[0].frames == 50u);
    /* Modifier has only a release sound: alpha's key-downs, its own release. */
    KF_EXPECT(kf_pack_sounds(p, KF_GROUP_MODIFIER, false, KF_TIER_SOFT, &takes) == 2u);
    KF_EXPECT(kf_pack_sounds(p, KF_GROUP_MODIFIER, true, 0u, &takes) == 1u &&
              takes[0].frames == 60u);
    /* Fallbacks share audio: no copies. */
    uint32_t a = 0u;
    uint32_t b = 0u;
    KF_EXPECT(first_sample(p, KF_GROUP_ENTER, false, KF_TIER_HARD, &a) ==
              first_sample(p, KF_GROUP_ALPHA, false, KF_TIER_HARD, &b));
    KF_EXPECT(kf_pack_decoded_bytes(p) == (size_t)(100 + 300 + 50 + 400 + 60 + 101) * 2);
    KF_EXPECT(kf_pack_sample_rate(p) == RATE);
    /* Out-of-range lookups play nothing. */
    KF_EXPECT(kf_pack_sounds(p, KF_GROUP_COUNT, false, 0u, &takes) == 0u);
    KF_EXPECT(kf_pack_sounds(p, KF_GROUP_ALPHA, false, KF_TIER_COUNT, &takes) == 0u);
    KF_EXPECT(kf_pack_sounds(NULL, KF_GROUP_ALPHA, false, 0u, &takes) == 0u);
    kf_pack_free(p);
    kf_pack_free(NULL);
}

void test_pack_derived_flags(void) {
    kf_pack_pcm sounds[] = {sound(KF_GROUP_ALPHA, false, KF_TIER_MEDIUM, 10u),
                            sound(KF_GROUP_ALPHA, false, KF_TIER_SLAM, 10u)};
    sounds[1].derived = true;
    kf_pack *p = kf_pack_from_pcm(sounds, 2u, RATE, NULL, 0u);
    KF_EXPECT(p != NULL);
    KF_EXPECT(!kf_pack_is_derived(p, KF_GROUP_ALPHA, KF_TIER_MEDIUM));
    KF_EXPECT(kf_pack_is_derived(p, KF_GROUP_ALPHA, KF_TIER_SLAM));
    KF_EXPECT(!kf_pack_is_derived(p, KF_GROUP_ALPHA, KF_TIER_SOFT)); /* Falls back to medium. */
    KF_EXPECT(kf_pack_is_derived(p, KF_GROUP_TAB, KF_TIER_SLAM));    /* Tab falls back to alpha. */
    KF_EXPECT(!kf_pack_is_derived(p, KF_GROUP_COUNT, 0u));
    kf_pack_free(p);
}

void test_pack_rejects_bad_input(void) {
    char err[160];
    const kf_pack_pcm no_alpha[] = {sound(KF_GROUP_SPACE, false, KF_TIER_SOFT, 10u)};
    KF_EXPECT(kf_pack_from_pcm(no_alpha, 1u, RATE, err, sizeof err) == NULL);
    KF_EXPECT(strstr(err, "alpha") != NULL);
    const kf_pack_pcm release_only[] = {sound(KF_GROUP_ALPHA, true, 0u, 10u)};
    KF_EXPECT(kf_pack_from_pcm(release_only, 1u, RATE, err, sizeof err) == NULL);

    const uint32_t too_long = (uint32_t)(RATE * KF_PACK_MAX_FILE_MS / 1000.0) + 1000u;
    int16_t *long_buf = calloc(too_long, sizeof *long_buf);
    KF_EXPECT(long_buf != NULL);
    kf_pack_pcm long_sound = sound(KF_GROUP_ALPHA, false, KF_TIER_SOFT, too_long);
    long_sound.samples = long_buf;
    KF_EXPECT(kf_pack_from_pcm(&long_sound, 1u, RATE, err, sizeof err) == NULL);
    KF_EXPECT(strstr(err, "longer") != NULL);

    /* Over 8 MB in total: 64 sounds of 1.4 s at 48 kHz is 8.6 MB. */
    const uint32_t frames = (uint32_t)(RATE * 1.4);
    int16_t *big = calloc(frames, sizeof *big);
    kf_pack_pcm many[64];
    for (size_t i = 0u; i < 64u; ++i) {
        many[i] = sound(KF_GROUP_ALPHA, false, KF_TIER_SOFT, frames);
        many[i].samples = big;
    }
    KF_EXPECT(big != NULL && kf_pack_from_pcm(many, 64u, RATE, err, sizeof err) == NULL);
    KF_EXPECT(strstr(err, "KB") != NULL);
    free(big);
    free(long_buf);

    static kf_pack_pcm crowd[KF_PACK_MAX_FILES + 1u];
    for (size_t i = 0u; i <= KF_PACK_MAX_FILES; ++i) {
        crowd[i] = sound(KF_GROUP_ALPHA, false, KF_TIER_SOFT, 4u);
    }
    KF_EXPECT(kf_pack_from_pcm(crowd, KF_PACK_MAX_FILES + 1u, RATE, err, sizeof err) == NULL);
    kf_pack *full = kf_pack_from_pcm(crowd, KF_PACK_MAX_FILES, RATE, err, sizeof err);
    KF_EXPECT(full != NULL); /* Exactly at the limit is fine. */
    kf_pack_free(full);

    kf_pack_pcm bad = sound(KF_SOUND_SLOTS, false, 0u, 10u);
    KF_EXPECT(kf_pack_from_pcm(&bad, 1u, RATE, err, sizeof err) == NULL);
    bad = sound(KF_GROUP_ALPHA, false, KF_TIER_COUNT, 10u);
    KF_EXPECT(kf_pack_from_pcm(&bad, 1u, RATE, err, sizeof err) == NULL);
    bad = sound(KF_GROUP_ALPHA, false, 0u, 0u);
    KF_EXPECT(kf_pack_from_pcm(&bad, 1u, RATE, err, sizeof err) == NULL);
    bad = sound(KF_GROUP_ALPHA, false, 0u, 10u);
    KF_EXPECT(kf_pack_from_pcm(&bad, 1u, 1000.0, err, sizeof err) == NULL); /* Bad rate. */
    KF_EXPECT(kf_pack_from_pcm(NULL, 1u, RATE, err, sizeof err) == NULL);
    KF_EXPECT(kf_pack_from_pcm(&bad, 0u, RATE, NULL, 0u) == NULL); /* NULL err is fine. */
}

void test_pack_holds_the_boom(void) {
    char err[160];
    const kf_pack_pcm sounds[] = {
        sound(KF_GROUP_ALPHA, false, KF_TIER_SOFT, 100u),
        sound(KF_GROUP_ALPHA, true, 0u, 50u),
        sound(KF_SOUND_BOOM, false, 0u, 700u),
    };
    kf_pack *p = kf_pack_from_pcm(sounds, 3u, RATE, err, sizeof err);
    KF_EXPECT(p != NULL);
    const kf_pack_sound *takes = NULL;
    KF_EXPECT(kf_pack_sounds(p, KF_SOUND_BOOM, false, 0u, &takes) == 1u && takes[0].frames == 700u);
    KF_EXPECT(kf_pack_sounds(p, KF_SOUND_BOOM, true, 0u, &takes) == 0u);   /* No release. */
    KF_EXPECT(kf_pack_sounds(p, KF_SOUND_BOOM, false, 2u, &takes) == 0u);  /* One tier. */
    KF_EXPECT(kf_pack_sounds(p, KF_SOUND_SLOTS, false, 0u, &takes) == 0u); /* Out of range. */
    /* Keys never fall back to the boom: Enter without sounds of its own plays alpha's. */
    for (uint8_t t = 0u; t < (uint8_t)KF_TIER_COUNT; ++t) {
        KF_EXPECT(kf_pack_sounds(p, KF_GROUP_ENTER, false, t, &takes) == 1u &&
                  takes[0].frames == 100u);
    }
    KF_EXPECT(kf_pack_sounds(p, KF_GROUP_ENTER, true, 0u, &takes) == 1u && takes[0].frames == 50u);
    KF_EXPECT(kf_pack_decoded_bytes(p) == (100u + 50u + 700u) * sizeof(int16_t));
    kf_pack_free(p);

    /* A pack without the boom plays none, and the boom alone is not a pack. */
    p = kf_pack_from_pcm(sounds, 2u, RATE, err, sizeof err);
    KF_EXPECT(p != NULL && kf_pack_sounds(p, KF_SOUND_BOOM, false, 0u, &takes) == 0u);
    kf_pack_free(p);
    KF_EXPECT(kf_pack_from_pcm(&sounds[2], 1u, RATE, err, sizeof err) == NULL);
    KF_EXPECT(strstr(err, "alpha") != NULL);
    /* The boom has one tier and no release sound. */
    kf_pack_pcm bad[2] = {sounds[0], sound(KF_SOUND_BOOM, true, 0u, 10u)};
    KF_EXPECT(kf_pack_from_pcm(bad, 2u, RATE, err, sizeof err) == NULL);
    bad[1] = sound(KF_SOUND_BOOM, false, KF_TIER_HARD, 10u);
    KF_EXPECT(kf_pack_from_pcm(bad, 2u, RATE, err, sizeof err) == NULL);
}

/* ------------------------------------------------------------------ decoding (macOS) */

#if defined(__APPLE__)
#define TMP_DIR "build/test-tmp"

static void put_le(FILE *f, uint32_t value, int bytes) {
    for (int i = 0; i < bytes; ++i) {
        (void)fputc((int)((value >> (8u * (unsigned)i)) & 0xffu), f);
    }
}

/* Minimal RIFF/WAVE writer: PCM, `bits` 16 or 24, interleaved `channels`. */
static const char *write_wav(const char *name, uint32_t rate, uint16_t bits, uint16_t channels,
                             uint32_t frames) {
    static char path[256];
    (void)snprintf(path, sizeof path, "%s/%s", TMP_DIR, name);
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        return NULL;
    }
    const uint32_t block = (uint32_t)channels * bits / 8u;
    const uint32_t data = frames * block;
    fputs("RIFF", f);
    put_le(f, 36u + data, 4);
    fputs("WAVEfmt ", f);
    put_le(f, 16u, 4);
    put_le(f, 1u, 2); /* PCM */
    put_le(f, channels, 2);
    put_le(f, rate, 4);
    put_le(f, rate * block, 4);
    put_le(f, block, 2);
    put_le(f, bits, 2);
    fputs("data", f);
    put_le(f, data, 4);
    for (uint32_t i = 0u; i < frames * channels; ++i) {
        const int32_t v = (int32_t)((i % 100u) * 300u) - 15000; /* A sawtooth. */
        put_le(f, (uint32_t)(bits == 24u ? v * 256 : v), bits / 8);
    }
    (void)fclose(f);
    return path;
}
#endif

void test_pack_decodes_files(void) {
#if defined(__APPLE__)
    KF_EXPECT(mkdir("build", 0755) == 0 || errno == EEXIST);
    KF_EXPECT(mkdir(TMP_DIR, 0755) == 0 || errno == EEXIST);
    char paths[4][256];
    (void)snprintf(paths[0], 256, "%s", write_wav("a16_44k.wav", 44100u, 16u, 1u, 4410u));
    (void)snprintf(paths[1], 256, "%s", write_wav("a24_48k.wav", 48000u, 24u, 1u, 2400u));
    (void)snprintf(paths[2], 256, "%s", write_wav("stereo.wav", 48000u, 16u, 2u, 480u));
    (void)snprintf(paths[3], 256, "%s", write_wav("long.wav", 48000u, 16u, 1u, 48000u * 2u));
    const kf_pack_file good[] = {
        {KF_GROUP_ALPHA, 0u, KF_TIER_SOFT, false, paths[0]},
        {KF_GROUP_ALPHA, 1u, 0u, true, paths[1]},
    };
    char err[256] = "";
    kf_pack *p = kf_pack_load(good, 2u, 48000.0, err, sizeof err);
    KF_EXPECT(p != NULL);
    if (p != NULL) {
        const kf_pack_sound *takes = NULL;
        KF_EXPECT(kf_pack_sounds(p, KF_GROUP_ALPHA, false, KF_TIER_SOFT, &takes) == 1u);
        /* 0.1 s at 44.1 kHz resampled to 48 kHz: 4800 frames, give or take the filter. */
        KF_EXPECT(takes[0].frames >= 4780u && takes[0].frames <= 4816u);
        KF_EXPECT(kf_pack_sounds(p, KF_GROUP_ALPHA, true, 0u, &takes) == 1u);
        KF_EXPECT(takes[0].frames == 2400u);
        KF_EXPECT(takes[0].samples[1] > takes[0].samples[0]); /* The sawtooth rises. */
        KF_EXPECT(kf_pack_sample_rate(p) == 48000.0);
        kf_pack_free(p);
    }
    const kf_pack_file stereo[] = {{KF_GROUP_ALPHA, 0u, 0u, false, paths[2]}};
    KF_EXPECT(kf_pack_load(stereo, 1u, 48000.0, err, sizeof err) == NULL);
    KF_EXPECT(strstr(err, "mono") != NULL);
    const kf_pack_file too_long[] = {{KF_GROUP_ALPHA, 0u, 0u, false, paths[3]}};
    KF_EXPECT(kf_pack_load(too_long, 1u, 48000.0, err, sizeof err) == NULL);
    KF_EXPECT(strstr(err, "longer") != NULL);

    const char *text = TMP_DIR "/not-audio.wav";
    FILE *f = fopen(text, "w");
    if (f != NULL) {
        fputs("definitely not audio\n", f);
        (void)fclose(f);
    }
    const kf_pack_file garbage[] = {{KF_GROUP_ALPHA, 0u, 0u, false, text}};
    KF_EXPECT(kf_pack_load(garbage, 1u, 48000.0, err, sizeof err) == NULL);
    const kf_pack_file missing[] = {{KF_GROUP_ALPHA, 0u, 0u, false, TMP_DIR "/nope.wav"}};
    KF_EXPECT(kf_pack_load(missing, 1u, 48000.0, err, sizeof err) == NULL);
    KF_EXPECT(strstr(err, "nope.wav") != NULL);
    const kf_pack_file no_path[] = {{KF_GROUP_ALPHA, 0u, 0u, false, NULL}};
    KF_EXPECT(kf_pack_load(no_path, 1u, 48000.0, err, sizeof err) == NULL);
    KF_EXPECT(kf_pack_load(good, 2u, 100.0, err, sizeof err) == NULL); /* Bad device rate. */
#else
    char err[64];
    const kf_pack_file any[] = {{KF_GROUP_ALPHA, 0u, 0u, false, "x.wav"}};
    KF_EXPECT(kf_pack_load(any, 1u, 48000.0, err, sizeof err) == NULL);
#endif
}
