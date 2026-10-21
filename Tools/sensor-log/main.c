// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/*
 * sensor-log: records every input report from one HID device as CSV, with KEYDOWN/KEYUP
 * markers from a listen-only event tap in the same process (Milestone 0).
 *
 * Output, format 1. Lines starting with '#' hold metadata (machine, device, permissions,
 * then a summary at the end). Data rows are
 *
 *     t_ns,event,src_ts,data
 *
 *   t_ns    arrival time in ns on the kf_time clock (mach_absolute_time), read first thing
 *           in the callback
 *   event   REPORT, KEYDOWN or KEYUP
 *   src_ts  the source's own timestamp, unconverted: the HID report timestamp or
 *           CGEventGetTimestamp(). Kept raw on purpose: Milestone 0 measures its units by
 *           comparing it with t_ns (Tools/py/analyze.py) instead of assuming them.
 *   data    report bytes in lowercase hex; empty for key rows
 *
 * Privacy: key rows never contain key codes or characters, and the tap is listen-only.
 * Auto-repeat events are dropped and counted. Safety: the device is opened without seizing
 * it and no device property is changed.
 *
 * Exit status: 0 ok, 1 error, 2 usage, 3 no single matching device, 4 permission denied.
 */

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDDevice.h>
#include <IOKit/hid/IOHIDManager.h>
#include <dispatch/dispatch.h>
#include <mach/mach_time.h>

#include <kf/kf_time.h>

#include "kf_hid.h"
#include "kf_tool.h"

#define EXIT_USAGE      2
#define EXIT_NO_DEVICE  3
#define EXIT_PERMISSION 4

#define FORMAT_VERSION   1
#define MAX_SECONDS      86400u
#define MAX_REPORT_BYTES 65536u
#define OUTPUT_BUFFER    (1u << 20)
#define NO_REPORT_HINT_S 3u
#define NS_PER_S         1000000000ULL
#define KEYS_TAIL_NS     500000000ULL /* Keep logging after the last key: its response. */
#define MAX_KEYS         100000u

static const char k_usage[] =
    "usage: sensor-log [matching flags] [options]\n"
    "\n"
    "Logs every input report of one HID device as CSV (t_ns,event,src_ts,data), with\n"
    "KEYDOWN/KEYUP rows from a listen-only event tap. Key rows hold no key codes.\n"
    "Stops on Ctrl-C or after --seconds. Opens the device read-only; changes nothing.\n"
    "\n"
    "Options:\n"
    "  --out FILE           write here ('-' for stdout, the default); never overwrites\n"
    "  --force              overwrite an existing --out file\n"
    "  --seconds N          stop after N seconds (1..86400; default: until Ctrl-C)\n"
    "  --stop-after-keys N  stop 0.5 s after the Nth key-down\n"
    "  --label TEXT         session tags saved in the header, e.g. surface=desk,force=rest\n"
    "  --no-keys            no event tap, so no Input Monitoring needed\n"
    "  --tap session|hid    event tap location (default: session)\n"
    "  --quiet              no live status on stderr\n"
    "\n"
    "Matching flags (at least one; must select exactly one device):\n" KF_TOOL_MATCH_USAGE "\n"
    "Example: sensor-log --vendor 0x05ac --product 0x8104 --usage-page 0xff00 --usage 3 \\\n"
    "             --seconds 10 --label surface=desk,force=rest --out logs/desk-rest.csv\n";

typedef enum tap_location { TAP_OFF, TAP_SESSION, TAP_HID } tap_location;

typedef struct options {
    kf_tool_match match;
    const char *out_path;
    const char *label;
    uint64_t seconds;
    uint64_t stop_after_keys;
    tap_location tap;
    bool force;
    bool quiet;
} options;

/* All state lives on the main thread: the device, the tap and every timer run on the main
 * run loop, so nothing here needs synchronization. */
typedef struct logger {
    FILE *out;
    char *row;
    uint8_t *report_buf;
    size_t report_cap;
    IOHIDDeviceRef device;
    CFMachPortRef tap;
    uint64_t start_ns;
    uint64_t reports;
    uint64_t report_errors;
    uint64_t oversize_reports;
    uint64_t keydown;
    uint64_t keyup;
    uint64_t autorepeat_dropped;
    uint64_t tap_reenabled;
    uint64_t stop_after_keys;
    uint64_t status_last_reports;
    uint32_t status_ticks;
    bool status_tty;
    bool hinted;
    const char *stop_reason;
} logger;

static logger g_log;

/* ------------------------------------------------------------------ options */

static bool label_is_valid(const char *label) {
    for (const unsigned char *p = (const unsigned char *)label; *p != '\0'; ++p) {
        if (*p < 0x20u || *p == 0x7fu) {
            return false;
        }
    }
    return strlen(label) <= 512u;
}

static int parse_options(int argc, char **argv, options *out) {
    memset(out, 0, sizeof *out);
    kf_tool_match_init(&out->match);
    out->out_path = "-";
    out->tap = TAP_SESSION;
    bool no_keys = false;
    for (int i = 1; i < argc; ++i) {
        const kf_tool_arg arg = kf_tool_match_parse(&out->match, argc, argv, &i);
        if (arg == KF_TOOL_ARG_TAKEN) {
            continue;
        }
        if (arg == KF_TOOL_ARG_BAD) {
            fprintf(stderr, "sensor-log: missing or invalid value for %s\n", argv[i]);
            return EXIT_USAGE;
        }
        const char *flag = argv[i];
        const char *value = i + 1 < argc ? argv[i + 1] : NULL;
        if (strcmp(flag, "--help") == 0 || strcmp(flag, "-h") == 0) {
            fputs(k_usage, stdout);
            return EXIT_SUCCESS;
        } else if (strcmp(flag, "--force") == 0) {
            out->force = true;
        } else if (strcmp(flag, "--quiet") == 0) {
            out->quiet = true;
        } else if (strcmp(flag, "--no-keys") == 0) {
            no_keys = true;
        } else if (strcmp(flag, "--out") == 0 && value != NULL && value[0] != '\0') {
            out->out_path = value;
            i++;
        } else if (strcmp(flag, "--label") == 0 && value != NULL && label_is_valid(value)) {
            out->label = value;
            i++;
        } else if (strcmp(flag, "--seconds") == 0 && value != NULL &&
                   kf_tool_parse_u64(value, MAX_SECONDS, &out->seconds) && out->seconds > 0u) {
            i++;
        } else if (strcmp(flag, "--stop-after-keys") == 0 && value != NULL &&
                   kf_tool_parse_u64(value, MAX_KEYS, &out->stop_after_keys) &&
                   out->stop_after_keys > 0u) {
            i++;
        } else if (strcmp(flag, "--tap") == 0 && value != NULL &&
                   (strcmp(value, "session") == 0 || strcmp(value, "hid") == 0)) {
            out->tap = strcmp(value, "hid") == 0 ? TAP_HID : TAP_SESSION;
            i++;
        } else {
            fprintf(stderr, "sensor-log: unknown option or bad value: %s\n\n%s", flag, k_usage);
            return EXIT_USAGE;
        }
    }
    if (kf_tool_match_is_empty(&out->match)) {
        fputs("sensor-log: give matching flags to choose the device (see hid-dump)\n", stderr);
        return EXIT_USAGE;
    }
    if (no_keys) {
        out->tap = TAP_OFF;
    }
    if (out->tap == TAP_OFF && out->stop_after_keys > 0u) {
        fputs("sensor-log: --stop-after-keys needs the event tap (drop --no-keys)\n", stderr);
        return EXIT_USAGE;
    }
    return -1; /* Continue. */
}

/* ------------------------------------------------------------------ rows */

/* One fwrite per row into a 1 MiB stdio buffer; no printf on the per-report path. */
static void write_row(uint64_t t_ns, const char *event, size_t event_len, uint64_t src_ts,
                      const uint8_t *data, size_t len) {
    char *p = g_log.row;
    p += kf_tool_u64_dec(p, t_ns);
    *p++ = ',';
    memcpy(p, event, event_len);
    p += event_len;
    *p++ = ',';
    p += kf_tool_u64_dec(p, src_ts);
    *p++ = ',';
    p += kf_tool_hex(p, data, len);
    *p++ = '\n';
    (void)fwrite(g_log.row, 1u, (size_t)(p - g_log.row), g_log.out);
}

static void stop(const char *reason) {
    if (g_log.stop_reason == NULL) {
        g_log.stop_reason = reason;
    }
    CFRunLoopStop(CFRunLoopGetMain());
}

static void on_report(void *ctx, IOReturn result, void *sender, IOHIDReportType type,
                      uint32_t report_id, uint8_t *report, CFIndex length, uint64_t timestamp) {
    const uint64_t now = kf_time_now_ns();
    (void)ctx;
    (void)sender;
    (void)type;
    (void)report_id;
    if (result != kIOReturnSuccess) {
        g_log.report_errors++;
        return;
    }
    if (length < 0 || (size_t)length > g_log.report_cap) {
        g_log.oversize_reports++;
        return;
    }
    write_row(now, "REPORT", 6u, timestamp, report, (size_t)length);
    g_log.reports++;
}

static void on_removed(void *ctx, IOReturn result, void *sender) {
    (void)ctx;
    (void)result;
    (void)sender;
    stop("device-removed");
}

static void on_keys_done(void *ctx) {
    (void)ctx;
    stop("keys");
}

static CGEventRef on_key(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *ctx) {
    const uint64_t now = kf_time_now_ns();
    (void)proxy;
    (void)ctx;
    if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
        g_log.tap_reenabled++;
        CGEventTapEnable(g_log.tap, true);
        return event;
    }
    if (type != kCGEventKeyDown && type != kCGEventKeyUp) {
        return event;
    }
    if (CGEventGetIntegerValueField(event, kCGKeyboardEventAutorepeat) != 0) {
        g_log.autorepeat_dropped++;
        return event;
    }
    /* Only the direction and the timestamp leave this callback: never the key code. */
    const bool down = type == kCGEventKeyDown;
    write_row(now, down ? "KEYDOWN" : "KEYUP", down ? 7u : 5u, CGEventGetTimestamp(event), NULL,
              0u);
    if (down) {
        g_log.keydown++;
        if (g_log.keydown == g_log.stop_after_keys) {
            dispatch_after_f(dispatch_time(DISPATCH_TIME_NOW, (int64_t)KEYS_TAIL_NS),
                             dispatch_get_main_queue(), NULL, on_keys_done);
        }
    } else {
        g_log.keyup++;
    }
    return event;
}

/* ------------------------------------------------------------------ timers and signals */

static void on_signal(void *ctx) {
    (void)ctx;
    stop("signal");
}

static void on_deadline(void *ctx) {
    (void)ctx;
    stop("seconds");
}

static void on_status(void *ctx) {
    (void)ctx;
    g_log.status_ticks++;
    const uint64_t rate = g_log.reports - g_log.status_last_reports;
    g_log.status_last_reports = g_log.reports;
    if (g_log.status_tty) {
        fprintf(stderr, "\r  %5u s  reports %9llu  (%4llu/s)  keys %llu down %llu up   ",
                g_log.status_ticks, (unsigned long long)g_log.reports, (unsigned long long)rate,
                (unsigned long long)g_log.keydown, (unsigned long long)g_log.keyup);
    }
    if (!g_log.hinted && g_log.reports == 0u && g_log.status_ticks >= NO_REPORT_HINT_S) {
        g_log.hinted = true;
        fprintf(stderr,
                "%ssensor-log: no reports after %u s. The device may be idle or "
                "powered down, or access may need more privileges. Keep logging or "
                "stop with Ctrl-C.\n",
                g_log.status_tty ? "\n" : "", NO_REPORT_HINT_S);
    }
}

static dispatch_source_t make_signal_source(int signo) {
    (void)signal(signo, SIG_IGN); /* Delivered through the dispatch source instead. */
    dispatch_source_t source = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, (uintptr_t)signo,
                                                      0u, dispatch_get_main_queue());
    if (source != NULL) {
        dispatch_source_set_event_handler_f(source, on_signal);
        dispatch_resume(source);
    }
    return source;
}

/* ------------------------------------------------------------------ header */

static void write_header(const options *opt, const kf_hid_info *d, IOReturn open_result,
                         bool listen_access) {
    FILE *f = g_log.out;
    char model[64];
    char os[128];
    char started[32];
    kf_hid_hw_model(model, sizeof model);
    kf_hid_os_version(os, sizeof os);
    const time_t now = time(NULL);
    struct tm utc;
    if (gmtime_r(&now, &utc) == NULL ||
        strftime(started, sizeof started, "%Y-%m-%dT%H:%M:%SZ", &utc) == 0u) {
        (void)snprintf(started, sizeof started, "unknown");
    }
    mach_timebase_info_data_t timebase = {0u, 0u};
    (void)mach_timebase_info(&timebase);

    fprintf(f, "# keyfeel sensor-log format=%d\n", FORMAT_VERSION);
    fprintf(f, "# started_utc=%s\n# hw_model=%s\n# os=%s\n", started, model, os);
    fprintf(f, "# mach_timebase=%u/%u\n# euid=%u\n", timebase.numer, timebase.denom,
            (unsigned)geteuid());
    fprintf(f, "# label=%s\n", opt->label != NULL ? opt->label : "");
    fprintf(f, "# device.product=%s\n# device.manufacturer=%s\n# device.transport=%s\n", d->product,
            d->manufacturer, d->transport);
    fprintf(f, "# device.vendor_id=0x%04llx\n# device.product_id=0x%04llx\n",
            (unsigned long long)(d->vendor_id < 0 ? 0 : d->vendor_id),
            (unsigned long long)(d->product_id < 0 ? 0 : d->product_id));
    fprintf(f, "# device.primary_usage=%04llx:%04llx\n# device.usage_pairs=",
            (unsigned long long)(d->usage_page < 0 ? 0 : d->usage_page),
            (unsigned long long)(d->usage < 0 ? 0 : d->usage));
    const uint32_t pairs = d->pair_count < KF_HID_MAX_PAIRS ? d->pair_count : KF_HID_MAX_PAIRS;
    for (uint32_t i = 0u; i < pairs; ++i) {
        fprintf(f, "%s%04x:%04x", i == 0u ? "" : " ", d->pairs[i][0], d->pairs[i][1]);
    }
    fprintf(f, "\n# device.built_in=%d\n# device.max_input_report=%lld\n", d->built_in,
            (long long)d->max_input_report);
    fprintf(f, "# device.report_interval_us=%lld\n# device.registry_id=0x%llx\n",
            (long long)d->report_interval_us, (unsigned long long)d->registry_id);
    fprintf(f, "# device.class=%s\n# device.path=%s\n", d->io_class, d->path);
    const size_t kept =
        d->descriptor_len < KF_HID_MAX_DESCRIPTOR ? d->descriptor_len : KF_HID_MAX_DESCRIPTOR;
    static char hex[2u * KF_HID_MAX_DESCRIPTOR + 1u];
    (void)kf_tool_hex(hex, d->descriptor, kept);
    fprintf(f, "# device.report_descriptor=%s\n", hex);
    fprintf(f, "# device.open=0x%08x %s\n", (unsigned)open_result,
            kf_hid_return_string(open_result));
    fprintf(f, "# keys.listen_access=%s\n", listen_access ? "granted" : "not-granted");
    fprintf(f, "# keys.tap=%s\n",
            opt->tap == TAP_OFF ? "off" : (opt->tap == TAP_HID ? "hid" : "session"));
    fputs("t_ns,event,src_ts,data\n", f);
}

static void write_footer(void) {
    const uint64_t duration = kf_time_now_ns() - g_log.start_ns;
    FILE *f = g_log.out;
    fprintf(f, "# end.reason=%s\n", g_log.stop_reason != NULL ? g_log.stop_reason : "unknown");
    fprintf(f, "# end.duration_ns=%llu\n", (unsigned long long)duration);
    fprintf(f, "# end.reports=%llu\n# end.report_errors=%llu\n# end.oversize_reports=%llu\n",
            (unsigned long long)g_log.reports, (unsigned long long)g_log.report_errors,
            (unsigned long long)g_log.oversize_reports);
    fprintf(f, "# end.keydown=%llu\n# end.keyup=%llu\n# end.autorepeat_dropped=%llu\n",
            (unsigned long long)g_log.keydown, (unsigned long long)g_log.keyup,
            (unsigned long long)g_log.autorepeat_dropped);
    fprintf(f, "# end.tap_reenabled=%llu\n", (unsigned long long)g_log.tap_reenabled);
}

/* ------------------------------------------------------------------ setup */

static int select_device(IOHIDManagerRef manager, const kf_tool_match *match, kf_hid_info *info,
                         IOHIDDeviceRef *out) {
    const CFArrayRef devices = kf_hid_copy_matching_devices(manager, match);
    const CFIndex count = devices != NULL ? CFArrayGetCount(devices) : 0;
    if (count != 1) {
        fprintf(stderr, "sensor-log: %ld devices match; the flags must select exactly one\n",
                (long)count);
        for (CFIndex i = 0; i < count; ++i) {
            kf_hid_info_read((IOHIDDeviceRef)(uintptr_t)CFArrayGetValueAtIndex(devices, i), info);
            fprintf(stderr, "  %s  usage %04llx:%04llx  transport %s  registry-id 0x%llx\n",
                    info->product, (unsigned long long)info->usage_page,
                    (unsigned long long)info->usage, info->transport,
                    (unsigned long long)info->registry_id);
        }
        if (devices != NULL) {
            CFRelease(devices);
        }
        return EXIT_NO_DEVICE;
    }
    *out = (IOHIDDeviceRef)(uintptr_t)CFArrayGetValueAtIndex(devices, 0);
    CFRetain(*out);
    CFRelease(devices);
    kf_hid_info_read(*out, info);
    return EXIT_SUCCESS;
}

static FILE *open_output(const options *opt) {
    if (strcmp(opt->out_path, "-") == 0) {
        return stdout;
    }
    /* "x" (C11): fail instead of silently replacing an earlier measurement. */
    FILE *f = fopen(opt->out_path, opt->force ? "w" : "wx");
    if (f == NULL) {
        fprintf(stderr, "sensor-log: cannot create %s: %s%s\n", opt->out_path, strerror(errno),
                errno == EEXIST ? " (use --force to overwrite)" : "");
    }
    return f;
}

static int create_tap(const options *opt) {
    const CGEventMask mask = CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp);
    g_log.tap =
        CGEventTapCreate(opt->tap == TAP_HID ? kCGHIDEventTap : kCGSessionEventTap,
                         kCGHeadInsertEventTap, kCGEventTapOptionListenOnly, mask, on_key, NULL);
    if (g_log.tap == NULL) {
        fputs("sensor-log: cannot create the event tap. Grant Input Monitoring to the app that\n"
              "  runs this tool (System Settings > Privacy & Security > Input Monitoring), then\n"
              "  start a new terminal session. Or use --no-keys to log reports only.\n",
              stderr);
        return EXIT_PERMISSION;
    }
    CFRunLoopSourceRef source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, g_log.tap, 0);
    if (source == NULL) {
        return EXIT_FAILURE;
    }
    CFRunLoopAddSource(CFRunLoopGetMain(), source, kCFRunLoopCommonModes);
    CFRelease(source);
    return EXIT_SUCCESS;
}

static void destroy_tap(void) {
    if (g_log.tap != NULL) {
        CGEventTapEnable(g_log.tap, false);
        CFMachPortInvalidate(g_log.tap);
        CFRelease(g_log.tap);
        g_log.tap = NULL;
    }
}

static void print_summary(void) {
    const double seconds = (double)(kf_time_now_ns() - g_log.start_ns) / (double)NS_PER_S;
    fprintf(stderr,
            "%ssensor-log: %.1f s, %llu reports (%.1f/s), %llu errors, %llu key-downs, "
            "%llu key-ups, %llu repeats dropped, %llu tap re-enables (%s)\n",
            g_log.status_tty ? "\n" : "", seconds, (unsigned long long)g_log.reports,
            seconds > 0.0 ? (double)g_log.reports / seconds : 0.0,
            (unsigned long long)(g_log.report_errors + g_log.oversize_reports),
            (unsigned long long)g_log.keydown, (unsigned long long)g_log.keyup,
            (unsigned long long)g_log.autorepeat_dropped, (unsigned long long)g_log.tap_reenabled,
            g_log.stop_reason != NULL ? g_log.stop_reason : "unknown");
}

/* ------------------------------------------------------------------ main */

static int run(const options *opt, IOHIDDeviceRef device, const kf_hid_info *info) {
    const int64_t max_report = info->max_input_report;
    g_log.report_cap =
        max_report > 0 && max_report <= (int64_t)MAX_REPORT_BYTES ? (size_t)max_report : 64u;
    g_log.report_buf = calloc(g_log.report_cap, 1u);
    /* t_ns + event + src_ts + hex + separators. */
    g_log.row = malloc(2u * KF_TOOL_U64_DIGITS + 16u + 2u * g_log.report_cap);
    if (g_log.report_buf == NULL || g_log.row == NULL) {
        fputs("sensor-log: out of memory\n", stderr);
        return EXIT_FAILURE;
    }

    const bool listen_access = CGPreflightListenEventAccess();
    if (opt->tap != TAP_OFF) {
        const int status = create_tap(opt);
        if (status != EXIT_SUCCESS) {
            return status;
        }
    }

    const IOReturn open_result = IOHIDDeviceOpen(device, kIOHIDOptionsTypeNone);
    if (open_result != kIOReturnSuccess) {
        fprintf(stderr, "sensor-log: IOHIDDeviceOpen failed: 0x%08x (%s), euid %u\n",
                (unsigned)open_result, kf_hid_return_string(open_result), (unsigned)geteuid());
        destroy_tap();
        return open_result == kIOReturnNotPermitted || open_result == kIOReturnNotPrivileged
                   ? EXIT_PERMISSION
                   : EXIT_FAILURE;
    }

    g_log.out = open_output(opt);
    if (g_log.out == NULL) {
        IOHIDDeviceClose(device, kIOHIDOptionsTypeNone);
        destroy_tap();
        return EXIT_FAILURE;
    }
    (void)setvbuf(g_log.out, NULL, _IOFBF, OUTPUT_BUFFER);
    write_header(opt, info, open_result, listen_access);

    g_log.device = device;
    g_log.stop_after_keys = opt->stop_after_keys;
    g_log.status_tty = !opt->quiet && isatty(STDERR_FILENO) != 0;
    IOHIDDeviceRegisterInputReportWithTimeStampCallback(device, g_log.report_buf,
                                                        (CFIndex)g_log.report_cap, on_report, NULL);
    IOHIDDeviceRegisterRemovalCallback(device, on_removed, NULL);
    IOHIDDeviceScheduleWithRunLoop(device, CFRunLoopGetMain(), kCFRunLoopDefaultMode);

    dispatch_source_t sigint = make_signal_source(SIGINT);
    dispatch_source_t sigterm = make_signal_source(SIGTERM);
    dispatch_source_t status = NULL;
    if (!opt->quiet) {
        status =
            dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0u, 0u, dispatch_get_main_queue());
        if (status != NULL) {
            dispatch_source_set_timer(status, dispatch_time(DISPATCH_TIME_NOW, (int64_t)NS_PER_S),
                                      NS_PER_S, NS_PER_S / 10u);
            dispatch_source_set_event_handler_f(status, on_status);
            dispatch_resume(status);
        }
    }
    if (opt->seconds > 0u) {
        dispatch_after_f(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(opt->seconds * NS_PER_S)),
                         dispatch_get_main_queue(), NULL, on_deadline);
    }

    if (!opt->quiet) {
        fprintf(stderr, "sensor-log: logging %s (%s) to %s%s\n", info->product, info->transport,
                strcmp(opt->out_path, "-") == 0 ? "stdout" : opt->out_path,
                opt->seconds > 0u ? "" : "; Ctrl-C stops");
    }
    g_log.start_ns = kf_time_now_ns();
    CFRunLoopRun();

    IOHIDDeviceRegisterInputReportWithTimeStampCallback(device, g_log.report_buf,
                                                        (CFIndex)g_log.report_cap, NULL, NULL);
    IOHIDDeviceUnscheduleFromRunLoop(device, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
    IOHIDDeviceClose(device, kIOHIDOptionsTypeNone);
    destroy_tap();
    dispatch_source_t sources[] = {status, sigint, sigterm};
    for (size_t i = 0u; i < sizeof sources / sizeof sources[0]; ++i) {
        if (sources[i] != NULL) {
            dispatch_source_cancel(sources[i]);
            dispatch_release(sources[i]); /* Plain C: no ARC for dispatch objects. */
        }
    }

    write_footer();
    if (!opt->quiet) {
        print_summary();
    }
    const bool write_failed = fflush(g_log.out) != 0 || ferror(g_log.out) != 0;
    if (g_log.out != stdout && fclose(g_log.out) != 0) {
        fprintf(stderr, "sensor-log: closing %s failed: %s\n", opt->out_path, strerror(errno));
        return EXIT_FAILURE;
    }
    if (write_failed) {
        fputs("sensor-log: writing the log failed\n", stderr);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    options opt;
    const int parsed = parse_options(argc, argv, &opt);
    if (parsed >= 0) {
        return parsed;
    }
    kf_time_init();

    IOHIDManagerRef manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    if (manager == NULL) {
        fputs("sensor-log: could not create the HID manager\n", stderr);
        return EXIT_FAILURE;
    }
    static kf_hid_info info; /* Large: keep it off the stack. */
    IOHIDDeviceRef device = NULL;
    int status = select_device(manager, &opt.match, &info, &device);
    if (status == EXIT_SUCCESS) {
        status = run(&opt, device, &info);
        CFRelease(device);
    }
    free(g_log.row);
    free(g_log.report_buf);
    CFRelease(manager);
    return status;
}
