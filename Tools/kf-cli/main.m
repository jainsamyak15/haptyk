// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/*
 * kf-cli: plays a sound pack while you type, to test the pipeline without the app (M1).
 * --demo plays scripted keystrokes through the same classification and routing instead of the
 * event tap, so it needs no Input Monitoring. --calibrate runs the calibration flow (M2); it
 * needs the motion sensor. Prints the engine's counters on exit. Changes no system setting:
 * the 128-frame buffer request applies to this process only.
 *
 * Exit status: 0 ok, 1 error, 2 usage, 3 sensor unavailable, 4 permission missing.
 */

#import <Foundation/Foundation.h>

#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

#include <kf/kf.h>

#import "Packs/KFPackManifest.h"
#include "keymap/kf_vk.h"
#include "keytap/kf_keytap_internal.h"

#define EXIT_USAGE 2
#define EXIT_NO_SENSOR 3
#define EXIT_PERMISSION 4

static NSString *const kDefaultPack = @"build/packs/Test-Click.kfpack";

static const char kUsage[] =
    "usage: kf-cli [--pack DIR] [--seconds N] [--volume V] [--width W] [--repeat]\n"
    "              [--demo N] [--calibrate]\n"
    "\n"
    "Plays a sound pack while you type. Needs Input Monitoring for the terminal app,\n"
    "except with --demo. Ctrl-C stops and prints the engine's counters.\n"
    "\n"
    "  --pack DIR    .kfpack folder (default: build/packs/Test-Click.kfpack, `make test-pack`)\n"
    "  --seconds N   stop after N seconds\n"
    "  --volume V    0..1 (default 0.8)\n"
    "  --width W     stereo width 0..1 (default 1)\n"
    "  --repeat      play auto-repeat too\n"
    "  --demo N      play N scripted keystrokes, 10 a second, instead of the key tap\n"
    "  --idle S      stop audio after S seconds without keys; the next key restarts it\n"
    "  --calibrate   calibration flow (needs the motion sensor)\n";

typedef struct options {
    const char *pack;
    double seconds;
    float volume;
    float width;
    bool repeat;
    long demo;
    double idle;
    bool calibrate;
} options;

static bool parse_number(const char *text, double lo, double hi, double *out) {
    char *end = NULL;
    const double value = text != NULL ? strtod(text, &end) : 0.0;
    if (text == NULL || end == text || *end != '\0' || !(value >= lo && value <= hi)) {
        return false;
    }
    *out = value;
    return true;
}

static int parse(int argc, const char **argv, options *opt) {
    *opt = (options){.pack = NULL, .seconds = 0.0, .volume = 0.8f, .width = 1.0f};
    for (int i = 1; i < argc; ++i) {
        const char *flag = argv[i];
        const char *value = i + 1 < argc ? argv[i + 1] : NULL;
        double number = 0.0;
        if (strcmp(flag, "--help") == 0 || strcmp(flag, "-h") == 0) {
            fputs(kUsage, stdout);
            return EXIT_SUCCESS;
        } else if (strcmp(flag, "--repeat") == 0) {
            opt->repeat = true;
        } else if (strcmp(flag, "--calibrate") == 0) {
            opt->calibrate = true;
        } else if (strcmp(flag, "--pack") == 0 && value != NULL) {
            opt->pack = value;
            i++;
        } else if (strcmp(flag, "--seconds") == 0 && parse_number(value, 1.0, 86400.0, &number)) {
            opt->seconds = number;
            i++;
        } else if (strcmp(flag, "--volume") == 0 && parse_number(value, 0.0, 1.0, &number)) {
            opt->volume = (float)number;
            i++;
        } else if (strcmp(flag, "--width") == 0 && parse_number(value, 0.0, 1.0, &number)) {
            opt->width = (float)number;
            i++;
        } else if (strcmp(flag, "--idle") == 0 && parse_number(value, 1.0, 3600.0, &number)) {
            opt->idle = number;
            i++;
        } else if (strcmp(flag, "--demo") == 0 && parse_number(value, 1.0, 1e6, &number)) {
            opt->demo = (long)number;
            i++;
        } else {
            fprintf(stderr, "kf-cli: unknown option or bad value: %s\n\n%s", flag, kUsage);
            return EXIT_USAGE;
        }
    }
    return -1;
}

/* ------------------------------------------------------------------ pack loading */

static KFPackManifest *g_manifest;

static bool load_pack(double rate) {
    NSError *error = nil;
    const uint64_t start = kf_time_now_ns();
    kf_pack *pack = [g_manifest loadPackAtSampleRate:rate error:&error];
    if (pack == NULL) {
        fprintf(stderr, "kf-cli: %s\n", error.localizedDescription.UTF8String);
        return false;
    }
    printf("kf-cli: %s, %lu files, %.0f KB decoded at %.0f Hz in %.1f ms\n",
           g_manifest.name.UTF8String, (unsigned long)g_manifest.fileCount,
           (double)kf_pack_decoded_bytes(pack) / 1024.0, rate,
           (double)(kf_time_now_ns() - start) / 1e6);
    kf_engine_set_pack(pack);
    return true;
}

/* The output device or its rate changed: restart at the new rate with the pack re-decoded,
 * so playback never resamples. */
static void on_device_change(void *ctx) {
    (void)ctx;
    double rate = 0.0;
    kf_engine_stop();
    if (kf_engine_default_output_rate(&rate) == KF_OK && load_pack(rate) &&
        kf_engine_start() == KF_OK) {
        char name[128];
        kf_engine_output_device_name(name, sizeof name);
        printf("kf-cli: output changed to %s at %.0f Hz\n", name, rate);
    } else {
        fprintf(stderr, "kf-cli: could not restart audio after a device change\n");
    }
}

/* ------------------------------------------------------------------ idle
 *
 * The power pattern of the spec ("Power state machine"): the key callback stores the last key
 * time in an atomic and wakes the main thread only for the first key after idle. While audio
 * runs, a main-thread timer checks every half timeout; while idle nothing runs at all. */

static _Atomic(uint64_t) g_last_key_ns;
static atomic_bool g_wake_requested;
static double g_idle_s;
static dispatch_source_t g_idle_timer;

static void stop_idle_timer(void) {
    if (g_idle_timer != nil) {
        dispatch_source_cancel(g_idle_timer);
        g_idle_timer = nil;
    }
}

static void check_idle(void) {
    const uint64_t idle_ns = (uint64_t)(g_idle_s * 1e9);
    if (kf_time_now_ns() - atomic_load_explicit(&g_last_key_ns, memory_order_relaxed) >= idle_ns) {
        stop_idle_timer();
        kf_engine_stop();
        printf("kf-cli: idle for %.0f s, audio stopped\n", g_idle_s);
    }
}

static void start_idle_timer(void) {
    if (g_idle_s <= 0.0 || g_idle_timer != nil) {
        return;
    }
    const uint64_t half = (uint64_t)(g_idle_s * 0.5 * NSEC_PER_SEC);
    g_idle_timer =
        dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    dispatch_source_set_timer(g_idle_timer, dispatch_time(DISPATCH_TIME_NOW, (int64_t)half), half,
                              half / 10u);
    dispatch_source_set_event_handler(g_idle_timer, ^{
        check_idle();
    });
    dispatch_resume(g_idle_timer);
}

static void wake_audio(void) {
    atomic_store_explicit(&g_wake_requested, false, memory_order_relaxed);
    if (!kf_engine_is_running() && kf_engine_start() == KF_OK) {
        puts("kf-cli: key pressed, audio started");
    }
    start_idle_timer();
}

/* Keytap thread. Must return quickly: two atomics, and a dispatch only after idle. */
static void on_key(const kf_key_event *event, void *ctx) {
    (void)ctx;
    atomic_store_explicit(&g_last_key_ns, event->t_ns, memory_order_relaxed);
    if (!kf_engine_is_running() &&
        !atomic_exchange_explicit(&g_wake_requested, true, memory_order_relaxed)) {
        dispatch_async(dispatch_get_main_queue(), ^{
            wake_audio();
        });
    }
}

/* ------------------------------------------------------------------ demo */

static const uint16_t kDemoKeys[] = {
    KF_VK_H, KF_VK_E, KF_VK_L, KF_VK_L,      KF_VK_O,   KF_VK_SPACE,  KF_VK_W,     KF_VK_O,
    KF_VK_R, KF_VK_L, KF_VK_D, KF_VK_RETURN, KF_VK_TAB, KF_VK_DELETE, KF_VK_SHIFT, KF_VK_DIGIT_1};

/* The main queue stands in for the keytap thread: it is the only producer of queue A. */
static void run_demo(long count, dispatch_block_t done) {
    __block long played = 0;
    dispatch_source_t timer =
        dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    dispatch_source_set_timer(timer, dispatch_time(DISPATCH_TIME_NOW, 0), 100 * NSEC_PER_MSEC,
                              NSEC_PER_MSEC);
    dispatch_source_set_event_handler(timer, ^{
        const uint16_t key = kDemoKeys[played % (long)(sizeof kDemoKeys / sizeof kDemoKeys[0])];
        kf_keytap_context()->callback = on_key;
        kf_keytap_handle_key(kf_keytap_context(), key, true, kf_time_now_ns());
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 60 * NSEC_PER_MSEC),
                       dispatch_get_main_queue(), ^{
                           kf_keytap_handle_key(kf_keytap_context(), key, false, kf_time_now_ns());
                       });
        if (++played >= count) {
            dispatch_source_cancel(timer);
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 500 * NSEC_PER_MSEC),
                           dispatch_get_main_queue(), done);
        }
    });
    dispatch_resume(timer);
}

/* ------------------------------------------------------------------ calibration
 *
 * The wizard math of the spec (velocity.c): noise floor for 3 s at rest, then about 40 soft
 * and 40 hard keystrokes; kf_calibration_compute() fits soft_g, slam_g and the row gains. */

#define CALIBRATION_KEYS 40u
#define NOISE_NS         3000000000ULL

typedef enum { CAL_NOISE, CAL_SOFT, CAL_HARD, CAL_DONE } cal_phase;

static kf_calibration_sample g_soft[CALIBRATION_KEYS];
static kf_calibration_sample g_hard[CALIBRATION_KEYS];

static void print_calibration(const kf_calibration *cal) {
    printf("{\"noise_floor_g\": %.5f, \"soft_g\": %.5f, \"slam_g\": %.5f, \"row_gain\": [",
           (double)cal->noise_floor_g, (double)cal->soft_g, (double)cal->slam_g);
    for (int r = 0; r < KF_ROW_COUNT; ++r) {
        printf("%s%.3f", r == 0 ? "" : ", ", (double)cal->row_gain[r]);
    }
    printf("], \"auto_adapt\": %s}\n", cal->auto_adapt ? "true" : "false");
}

static void run_calibration(dispatch_block_t done) {
    __block cal_phase phase = CAL_NOISE;
    __block uint32_t soft = 0u;
    __block uint32_t hard = 0u;
    puts("Calibration 1/3: hands off the Mac for 3 seconds...");
    kf_sensor_begin_noise_measurement(NOISE_NS);
    dispatch_source_t timer =
        dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    dispatch_source_set_timer(timer, dispatch_time(DISPATCH_TIME_NOW, 0), 100 * NSEC_PER_MSEC,
                              10 * NSEC_PER_MSEC);
    dispatch_source_set_event_handler(timer, ^{
      kf_sensor_peak peak;
      if (phase == CAL_NOISE) {
          if (kf_sensor_noise_floor_g() >= 0.0f) {
              printf("  noise floor %.4f g\nCalibration 2/3: type softly, about %u keys...\n",
                     (double)kf_sensor_noise_floor_g(), CALIBRATION_KEYS);
              while (kf_sensor_pop_peak(&peak)) {
              }
              phase = CAL_SOFT;
          }
          return;
      }
      while (phase != CAL_DONE && kf_sensor_pop_peak(&peak)) {
          kf_calibration_sample *set = phase == CAL_SOFT ? g_soft : g_hard;
          uint32_t *n = phase == CAL_SOFT ? &soft : &hard;
          set[(*n)++] = (kf_calibration_sample){peak.raw_g, peak.row};
          if (phase == CAL_SOFT && soft == CALIBRATION_KEYS) {
              printf("Calibration 3/3: now type hard, about %u keys...\n", CALIBRATION_KEYS);
              phase = CAL_HARD;
          } else if (phase == CAL_HARD && hard == CALIBRATION_KEYS) {
              phase = CAL_DONE;
          }
      }
      if (phase != CAL_DONE) {
          return;
      }
      dispatch_source_cancel(timer);
      kf_calibration cal = kf_calibration_default();
      const kf_status status = kf_calibration_compute(kf_sensor_noise_floor_g(), g_soft, soft,
                                                      g_hard, hard, &cal);
      if (status == KF_OK && kf_sensor_set_calibration(&cal) == KF_OK) {
          puts("Calibration done; in use now. As stored by the app (calibration.v1):");
          print_calibration(&cal);
      } else {
          printf("Calibration failed (%s): soft and hard taps did not separate. Try again with "
                 "softer soft taps and harder hard ones.\n",
                 kf_status_string(status));
      }
      done();
    });
    dispatch_resume(timer);
}

/* ------------------------------------------------------------------ main */

static void print_counters(void) {
    kf_engine_counters e;
    kf_keytap_counters k;
    kf_engine_get_counters(&e);
    kf_keytap_get_counters(&k);
    printf("\nengine: %llu callbacks of %u frames%s; callback p50 %.0f us, p99 %.0f us, "
           "max %.1f us; %llu overruns\n",
           (unsigned long long)e.callbacks, e.granted_buffer_frames,
           e.sample_rate > 0.0 ? "" : " (audio now stopped)", (double)e.callback_p50_ns / 1e3,
           (double)e.callback_p99_ns / 1e3, (double)e.max_callback_ns / 1e3,
           (unsigned long long)e.callback_overruns);
    printf("        start to first callback %.1f ms; requests dropped %llu, stale %llu; voices "
           "stolen %llu\n",
           (double)e.start_latency_ns / 1e6, (unsigned long long)e.requests_dropped,
           (unsigned long long)e.requests_stale, (unsigned long long)e.voices_stolen);
    printf("keys:   %llu played, %llu auto-repeats dropped, %llu tap timeouts\n",
           (unsigned long long)k.events, (unsigned long long)k.autorepeat_dropped,
           (unsigned long long)k.tap_timeouts);
}

int main(int argc, const char **argv) {
    @autoreleasepool {
        options opt;
        const int parsed = parse(argc, argv, &opt);
        if (parsed >= 0) {
            return parsed;
        }
        kf_time_init();
        (void)kf_engine_init();
        if (opt.calibrate) {
            /* Calibration measures real taps with the motion sensor (spec: velocity.c). */
            if (!kf_sensor_probe()) {
                fputs("kf-cli: the motion sensor is not available on this Mac, so calibration "
                      "cannot run (docs/SENSOR_NOTES.md).\n",
                      stderr);
                return EXIT_NO_SENSOR;
            }
        }

        NSString *packPath = opt.pack != NULL ? @(opt.pack) : kDefaultPack;
        NSError *error = nil;
        g_manifest = [KFPackManifest manifestWithPackAtURL:[NSURL fileURLWithPath:packPath]
                                                     error:&error];
        if (g_manifest == nil) {
            fprintf(stderr, "kf-cli: %s: %s\n", packPath.UTF8String,
                    error.localizedDescription.UTF8String);
            return EXIT_FAILURE;
        }
        double rate = 0.0;
        if (kf_engine_default_output_rate(&rate) != KF_OK) {
            fputs("kf-cli: no audio output device\n", stderr);
            return EXIT_FAILURE;
        }
        if (!load_pack(rate)) {
            return EXIT_FAILURE;
        }
        kf_engine_set_volume(opt.volume);
        kf_engine_set_stereo_width(opt.width);
        kf_engine_set_change_handler(on_device_change, NULL);
        kf_keytap_set_repeat_sounds(opt.repeat);
        g_idle_s = opt.idle;
        atomic_store_explicit(&g_last_key_ns, kf_time_now_ns(), memory_order_relaxed);
        if (kf_engine_start() != KF_OK) {
            fputs("kf-cli: could not start audio output\n", stderr);
            return EXIT_FAILURE;
        }
        char device[128];
        kf_engine_output_device_name(device, sizeof device);
        kf_engine_counters counters;
        kf_engine_get_counters(&counters);
        printf("kf-cli: playing on %s, %u-frame buffer granted\n", device,
               counters.granted_buffer_frames);

        dispatch_block_t stop = ^{
            CFRunLoopStop(CFRunLoopGetMain());
        };
        if (opt.calibrate) {
            const kf_status tap = kf_keytap_start(on_key, NULL, false);
            const kf_status sensor = tap == KF_OK ? kf_sensor_start() : tap;
            if (sensor != KF_OK) {
                fprintf(stderr, "kf-cli: cannot calibrate: %s\n", kf_status_string(sensor));
                kf_keytap_stop();
                kf_engine_stop();
                kf_engine_set_pack(NULL);
                return tap != KF_OK ? EXIT_PERMISSION : EXIT_NO_SENSOR;
            }
            run_calibration(stop);
        } else if (opt.demo > 0) {
            printf("kf-cli: demo, %ld keystrokes\n", opt.demo);
            /* With --seconds the run outlasts the demo, e.g. to measure the idle state. */
            run_demo(opt.demo, opt.seconds > 0.0 ? ^{
            } : stop);
        } else {
            const kf_status tap = kf_keytap_start(on_key, NULL, false);
            if (tap != KF_OK) {
                fprintf(stderr,
                        "kf-cli: no key tap (%s). Grant Input Monitoring to this terminal app in "
                        "System Settings > Privacy & Security, or use --demo.\n",
                        kf_status_string(tap));
                kf_engine_stop();
                kf_engine_set_pack(NULL);
                return EXIT_PERMISSION;
            }
            puts("kf-cli: type anywhere; Ctrl-C stops");
        }
        signal(SIGINT, SIG_IGN);
        dispatch_source_t sigint = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGINT, 0,
                                                          dispatch_get_main_queue());
        dispatch_source_set_event_handler(sigint, stop);
        dispatch_resume(sigint);
        start_idle_timer();
        if (opt.seconds > 0.0) {
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(opt.seconds * NSEC_PER_SEC)),
                           dispatch_get_main_queue(), stop);
        }
        CFRunLoopRun();

        stop_idle_timer();
        kf_keytap_stop();
        kf_sensor_stop();
        print_counters();
        kf_engine_stop();
        kf_engine_set_pack(NULL);
    }
    return EXIT_SUCCESS;
}
