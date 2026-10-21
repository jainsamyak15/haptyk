// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/*
 * mix-bench: the spec asks to benchmark the mixer's inner loop as auto-vectorized C against
 * Accelerate (vDSP) and keep the faster (docs/SPEC.md, "Optimization rules"). Mixes 24 voices
 * of 128 frames, int16 -> float, into a stereo pair, both ways, and checks they agree.
 *
 * The C loop is the one in Core/src/engine/kf_render_rt.c (mix_voice, constant gain).
 * Run on a release build: `make bench`.
 */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Accelerate/Accelerate.h>

#include <kf/kf_config.h>
#include <kf/kf_time.h>

#define FRAMES  KF_IO_BUFFER_FRAMES
#define VOICES  KF_MAX_VOICES
#define ROUNDS  200000u
#define REPEATS 5

static int16_t g_src[VOICES][FRAMES];
static float g_gain_l[VOICES];
static float g_gain_r[VOICES];

/* Same shape as kf_render_rt.c's constant-gain loop. */
static void mix_c(float *restrict left, float *restrict right) {
    for (uint32_t v = 0u; v < VOICES; ++v) {
        const int16_t *restrict src = g_src[v];
        const float gl = g_gain_l[v];
        const float gr = g_gain_r[v];
        for (uint32_t i = 0u; i < FRAMES; ++i) {
            const float x = (float)src[i];
            left[i] += x * gl;
            right[i] += x * gr;
        }
    }
}

static void mix_vdsp(float *restrict left, float *restrict right) {
    float tmp[FRAMES];
    for (uint32_t v = 0u; v < VOICES; ++v) {
        vDSP_vflt16(g_src[v], 1, tmp, 1, FRAMES);
        vDSP_vsma(tmp, 1, &g_gain_l[v], left, 1, left, 1, FRAMES);
        vDSP_vsma(tmp, 1, &g_gain_r[v], right, 1, right, 1, FRAMES);
    }
}

typedef void (*mixer_fn)(float *restrict, float *restrict);

/* Best of REPEATS, in ns per buffer: the least-disturbed run. */
static double time_mixer(mixer_fn fn, float *checksum) {
    static float left[FRAMES];
    static float right[FRAMES];
    double best = 1e30;
    float sum = 0.0f;
    for (int r = 0; r < REPEATS; ++r) {
        const uint64_t start = kf_time_now_ns();
        for (uint32_t round = 0u; round < ROUNDS; ++round) {
            memset(left, 0, sizeof left);
            memset(right, 0, sizeof right);
            fn(left, right);
            sum += left[round % FRAMES]; /* Keeps the work observable. */
        }
        const double ns = (double)(kf_time_now_ns() - start) / (double)ROUNDS;
        best = ns < best ? ns : best;
    }
    *checksum = sum;
    return best;
}

int main(void) {
    kf_time_init();
    for (uint32_t v = 0u; v < VOICES; ++v) {
        for (uint32_t i = 0u; i < FRAMES; ++i) {
            g_src[v][i] = (int16_t)((int32_t)((v * 131u + i * 17u) % 4000u) - 2000);
        }
        g_gain_l[v] = (0.5f + 0.02f * (float)v) / 32768.0f;
        g_gain_r[v] = (0.9f - 0.02f * (float)v) / 32768.0f;
    }
    static float cl[FRAMES], cr[FRAMES], vl[FRAMES], vr[FRAMES];
    mix_c(cl, cr);
    mix_vdsp(vl, vr);
    float worst = 0.0f;
    for (uint32_t i = 0u; i < FRAMES; ++i) {
        worst = fmaxf(worst, fmaxf(fabsf(cl[i] - vl[i]), fabsf(cr[i] - vr[i])));
    }
    if (worst > 1e-5f) {
        fprintf(stderr, "mix-bench: the two mixers disagree by %g\n", (double)worst);
        return EXIT_FAILURE;
    }
    float sink_c = 0.0f;
    float sink_v = 0.0f;
    const double c = time_mixer(mix_c, &sink_c);
    const double v = time_mixer(mix_vdsp, &sink_v);
    printf("mix-bench: %u voices x %u frames, best of %d x %u buffers\n", VOICES, FRAMES, REPEATS,
           ROUNDS);
    printf("  auto-vectorized C  %8.1f ns per buffer\n", c);
    printf("  Accelerate vDSP    %8.1f ns per buffer\n", v);
    printf("  faster: %s by %.0f%% (checksums %.3g, %.3g)\n", c <= v ? "C" : "vDSP",
           100.0 * fabs(c - v) / (c < v ? c : v), (double)sink_c, (double)sink_v);
    return EXIT_SUCCESS;
}
