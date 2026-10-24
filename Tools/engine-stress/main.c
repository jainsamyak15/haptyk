// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/*
 * engine-stress: runs the real output unit with N voices a second from a producer thread
 * (standing in for the keytap) while building and swapping in a fresh pack every 10 s, then
 * reports callback overruns, timing and memory growth (spec: "Testing and performance
 * verification"). Plays at -60 dB by default: inaudible, yet the mixer does all its work
 * (only true silence takes the shortcut).
 *
 * Exit status 1 on any overrun, dropped request, or more than 1 MB of memory growth.
 */

#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <mach/mach.h>

#include <kf/kf.h>

#define DEFAULT_MINUTES  10.0
#define DEFAULT_RATE     30.0
#define DEFAULT_VOLUME   0.001f
#define SWAP_EVERY_S     10u
#define MAX_GROWTH_BYTES (1024u * 1024u)
#define TAKE_MS          120u

static const char k_usage[] =
    "usage: engine-stress [--minutes M] [--voices-per-second N] [--volume V]\n"
    "Real audio output for M minutes (default 10) with N voices a second (default 30) and a\n"
    "new pack every 10 s. Default volume 0.001 (-60 dB): inaudible, fully mixed.\n";

static atomic_bool g_stop;

static uint64_t footprint(void) {
    task_vm_info_data_t info;
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO, (task_info_t)&info, &count) != KERN_SUCCESS) {
        return 0u;
    }
    return info.phys_footprint;
}

/* A synthetic pack: every group and tier, three takes each, release sounds. */
static kf_pack *make_pack(double rate, uint32_t seed) {
    enum { TAKES = 3 };
    const uint32_t frames = (uint32_t)(rate * TAKE_MS / 1000.0);
    static int16_t storage[KF_GROUP_COUNT][KF_TIER_COUNT + 1][TAKES][48000 * TAKE_MS / 1000 + 64];
    kf_pack_pcm sounds[KF_GROUP_COUNT * (KF_TIER_COUNT + 1) * TAKES];
    size_t n = 0u;
    for (uint8_t g = 0u; g < (uint8_t)KF_GROUP_COUNT; ++g) {
        for (uint8_t t = 0u; t <= (uint8_t)KF_TIER_COUNT; ++t) {
            for (int k = 0; k < TAKES; ++k) {
                int16_t *buf = storage[g][t][k];
                const double freq = 600.0 + 200.0 * g + 50.0 * k + (double)(seed % 7u) * 10.0;
                for (uint32_t i = 0u; i < frames; ++i) {
                    const double env = exp(-(double)i / (rate * 0.01));
                    buf[i] = (int16_t)(20000.0 * env * sin(2.0 * 3.14159265 * freq * i / rate));
                }
                sounds[n++] = (kf_pack_pcm){.samples = buf,
                                            .frames = frames,
                                            .group = g,
                                            .is_up = t == KF_TIER_COUNT ? 1u : 0u,
                                            .tier = t == KF_TIER_COUNT ? 0u : t,
                                            .derived = false};
            }
        }
    }
    char err[128];
    kf_pack *pack = kf_pack_from_pcm(sounds, n, rate, err, sizeof err);
    if (pack == NULL) {
        fprintf(stderr, "engine-stress: %s\n", err);
    }
    return pack;
}

typedef struct producer_args {
    double per_second;
} producer_args;

/* The keytap's role: the only producer of queue A. */
static void *producer(void *arg) {
    const producer_args *args = arg;
    (void)pthread_setname_np("engine-stress.producer");
    const uint64_t period = (uint64_t)(1e9 / args->per_second);
    uint32_t i = 0u;
    while (!atomic_load_explicit(&g_stop, memory_order_relaxed)) {
        const kf_key_event down = kf_keymap_make_event((uint16_t)(i * 7u % 0x7fu), (i & 1u) == 0u,
                                                       kf_time_now_ns(), true);
        (void)kf_emit_keystroke(&down, (float)(i % 11u) / 10.0f, KF_QUEUE_KEYTAP);
        i++;
        struct timespec ts = {0, (long)period};
        (void)nanosleep(&ts, NULL);
    }
    return NULL;
}

static void on_signal(int signo) {
    (void)signo;
    atomic_store(&g_stop, true);
}

/* strtod with full checking: the whole string must be one finite number (cert-err34-c). */
static bool parse_double(const char *s, double *out) {
    char *end = NULL;
    errno = 0;
    const double value = strtod(s, &end);
    if (end == s || *end != '\0' || errno != 0) {
        return false;
    }
    *out = value;
    return true;
}

int main(int argc, char **argv) {
    double minutes = DEFAULT_MINUTES;
    producer_args args = {DEFAULT_RATE};
    float volume = DEFAULT_VOLUME;
    for (int i = 1; i < argc; ++i) {
        const char *value = i + 1 < argc ? argv[i + 1] : "";
        double number = 0.0;
        if (strcmp(argv[i], "--minutes") == 0 && parse_double(value, &number) && number > 0.0) {
            minutes = number;
            i++;
        } else if (strcmp(argv[i], "--voices-per-second") == 0 && parse_double(value, &number) &&
                   number > 0.0) {
            args.per_second = number;
            i++;
        } else if (strcmp(argv[i], "--volume") == 0 && parse_double(value, &number) &&
                   number >= 0.0 && number <= 1.0) {
            volume = (float)number;
            i++;
        } else {
            fputs(k_usage, strcmp(argv[i], "--help") == 0 ? stdout : stderr);
            return strcmp(argv[i], "--help") == 0 ? EXIT_SUCCESS : 2;
        }
    }
    kf_time_init();
    (void)kf_engine_init();
    double rate = 0.0;
    if (kf_engine_default_output_rate(&rate) != KF_OK) {
        fputs("engine-stress: no audio output\n", stderr);
        return EXIT_FAILURE;
    }
    kf_engine_set_volume(volume);
    kf_pack *first = make_pack(rate, 0u);
    if (first == NULL) {
        return EXIT_FAILURE;
    }
    kf_engine_set_pack(first);
    if (kf_engine_start() != KF_OK) {
        fputs("engine-stress: cannot start audio\n", stderr);
        return EXIT_FAILURE;
    }
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    /* Warm-up: pools, the output unit and the first swap settle before memory is measured. */
    struct timespec second = {1, 0};
    (void)nanosleep(&second, NULL);
    kf_engine_set_pack(make_pack(rate, 1u));
    const uint64_t baseline = footprint();
    uint64_t peak = baseline;

    pthread_t thread;
    if (pthread_create(&thread, NULL, producer, &args) != 0) {
        return EXIT_FAILURE;
    }
    const uint64_t total_s = (uint64_t)(minutes * 60.0);
    uint64_t max_swap_ns = 0u;
    uint32_t swaps = 0u;
    printf("engine-stress: %.1f min, %.0f voices/s, pack swap every %u s, volume %.3f\n", minutes,
           args.per_second, SWAP_EVERY_S, (double)volume);
    for (uint64_t s = 1u; s <= total_s && !atomic_load(&g_stop); ++s) {
        (void)nanosleep(&second, NULL);
        if (s % SWAP_EVERY_S == 0u) {
            kf_pack *next = make_pack(rate, (uint32_t)s);
            const uint64_t t0 = kf_time_now_ns();
            kf_engine_set_pack(next);
            const uint64_t waited = kf_time_now_ns() - t0;
            max_swap_ns = waited > max_swap_ns ? waited : max_swap_ns;
            swaps++;
        }
        const uint64_t now = footprint();
        peak = now > peak ? now : peak;
        if (s % 60u == 0u) {
            printf("  %3llu min: footprint %.2f MB\n", (unsigned long long)(s / 60u),
                   (double)now / 1048576.0);
            fflush(stdout);
        }
    }
    atomic_store(&g_stop, true);
    (void)pthread_join(thread, NULL);
    const uint64_t end = footprint();

    kf_engine_counters c;
    kf_engine_get_counters(&c);
    kf_engine_stop();
    kf_engine_set_pack(NULL);
    const int64_t growth = (int64_t)end - (int64_t)baseline;
    printf("callbacks %llu at %.0f Hz, %u frames; p50 %.0f us, p99 %.0f us, max %.1f us\n",
           (unsigned long long)c.callbacks, c.sample_rate, c.granted_buffer_frames,
           (double)c.callback_p50_ns / 1e3, (double)c.callback_p99_ns / 1e3,
           (double)c.max_callback_ns / 1e3);
    printf("overruns %llu, dropped %llu, stale %llu, voices stolen %llu\n",
           (unsigned long long)c.callback_overruns, (unsigned long long)c.requests_dropped,
           (unsigned long long)c.requests_stale, (unsigned long long)c.voices_stolen);
    printf("pack swaps %u, slowest %.1f ms\n", swaps, (double)max_swap_ns / 1e6);
    printf("footprint %.2f MB -> %.2f MB (peak %.2f MB), growth %+.1f KB\n",
           (double)baseline / 1048576.0, (double)end / 1048576.0, (double)peak / 1048576.0,
           (double)growth / 1024.0);
    const bool ok =
        c.callback_overruns == 0u && c.requests_dropped == 0u && growth < (int64_t)MAX_GROWTH_BYTES;
    puts(ok ? "PASS" : "FAIL");
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
