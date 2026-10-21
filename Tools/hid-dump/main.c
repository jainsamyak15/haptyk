// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/*
 * hid-dump: lists HID devices through IOHIDManager to find the motion sensor (Milestone 0).
 * For each device it prints vendor and product IDs, product string, transport, primary usage,
 * the built-in flag, report sizes, report interval, every usage pair, the report descriptor and
 * the IORegistry class and path. It only reads: no device is opened or changed.
 *
 * --probe times the cheap presence check the app will use (IOServiceGetMatchingService with the
 * same matching keys) and checks it finds the same device as IOHIDManager.
 *
 * Exit status: 0 ok, 1 error, 2 usage, 3 nothing matched.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDKeys.h>

#include <kf/kf_time.h>

#include "kf_hid.h"
#include "kf_tool.h"

#define EXIT_USAGE      2
#define EXIT_NO_MATCH   3
#define PROBE_RUNS      200u
#define TEXT_DESC_BYTES 64u

static const char k_usage[] =
    "usage: hid-dump [--json] [--verbose] [--probe] [matching flags]\n"
    "\n"
    "Lists HID devices (all of them without matching flags). Read-only.\n"
    "\n"
    "  --json             JSON array instead of text\n"
    "  --verbose          full report descriptors in text output\n"
    "  --probe            time the presence check for the matching flags\n"
    "\n"
    "Matching flags:\n" KF_TOOL_MATCH_USAGE "\n"
    "Example: hid-dump --transport SPU\n";

typedef struct options {
    bool json;
    bool verbose;
    bool probe;
    kf_tool_match match;
} options;

static int parse_options(int argc, char **argv, options *out) {
    memset(out, 0, sizeof *out);
    kf_tool_match_init(&out->match);
    for (int i = 1; i < argc; ++i) {
        const kf_tool_arg arg = kf_tool_match_parse(&out->match, argc, argv, &i);
        if (arg == KF_TOOL_ARG_TAKEN) {
            continue;
        }
        if (arg == KF_TOOL_ARG_BAD) {
            fprintf(stderr, "hid-dump: missing or invalid value for %s\n", argv[i]);
            return EXIT_USAGE;
        }
        if (strcmp(argv[i], "--json") == 0) {
            out->json = true;
        } else if (strcmp(argv[i], "--verbose") == 0) {
            out->verbose = true;
        } else if (strcmp(argv[i], "--probe") == 0) {
            out->probe = true;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            fputs(k_usage, stdout);
            return EXIT_SUCCESS;
        } else {
            fprintf(stderr, "hid-dump: unknown option %s\n\n%s", argv[i], k_usage);
            return EXIT_USAGE;
        }
    }
    if (out->probe && kf_tool_match_is_empty(&out->match)) {
        fputs("hid-dump: --probe needs matching flags\n", stderr);
        return EXIT_USAGE;
    }
    if (out->probe && out->json) {
        fputs("hid-dump: --probe prints text; use it without --json\n", stderr);
        return EXIT_USAGE;
    }
    if (out->probe && out->match.product != NULL) {
        fputs("hid-dump: --probe cannot use --name: kernel matching ignores the product string\n",
              stderr);
        return EXIT_USAGE;
    }
    return -1; /* Continue. */
}

/* ------------------------------------------------------------------ text output */

static void print_hex_or_none(const char *label, int64_t value, int width) {
    if (value < 0) {
        printf("  %s -", label);
    } else {
        printf("  %s 0x%0*llx", label, width, (unsigned long long)value);
    }
}

static void print_text(const kf_hid_info *d, size_t index, bool verbose) {
    printf("#%zu  %s", index, d->product[0] != '\0' ? d->product : "(no product string)");
    if (d->manufacturer[0] != '\0') {
        printf("  (%s)", d->manufacturer);
    }
    printf("\n  ");
    print_hex_or_none("vendor", d->vendor_id, 4);
    print_hex_or_none("product", d->product_id, 4);
    printf("  transport %s  built-in %s", d->transport[0] != '\0' ? d->transport : "-",
           d->built_in < 0 ? "-" : (d->built_in != 0 ? "yes" : "no"));
    print_hex_or_none("location", d->location_id, 8);
    if (d->usage_page < 0 || d->usage < 0) {
        printf("\n    primary usage -  pairs");
    } else {
        printf("\n    primary usage %04llx:%04llx  pairs", (unsigned long long)d->usage_page,
               (unsigned long long)d->usage);
    }
    const uint32_t stored = d->pair_count < KF_HID_MAX_PAIRS ? d->pair_count : KF_HID_MAX_PAIRS;
    for (uint32_t i = 0u; i < stored; ++i) {
        printf(" %04x:%04x", d->pairs[i][0], d->pairs[i][1]);
    }
    if (d->pair_count > stored) {
        printf(" (+%u more)", d->pair_count - stored);
    }
    printf("\n    reports in %lld / out %lld / feature %lld bytes  interval %lld us\n",
           (long long)d->max_input_report, (long long)d->max_output_report,
           (long long)d->max_feature_report, (long long)d->report_interval_us);
    printf("    class %s  registry-id 0x%llx\n", d->io_class[0] != '\0' ? d->io_class : "-",
           (unsigned long long)d->registry_id);
    printf("    path %s\n", d->path[0] != '\0' ? d->path : "-");
    if (d->descriptor_len > 0u) {
        size_t shown =
            d->descriptor_len < KF_HID_MAX_DESCRIPTOR ? d->descriptor_len : KF_HID_MAX_DESCRIPTOR;
        if (!verbose && shown > TEXT_DESC_BYTES) {
            shown = TEXT_DESC_BYTES;
        }
        char hex[2u * KF_HID_MAX_DESCRIPTOR + 1u];
        (void)kf_tool_hex(hex, d->descriptor, shown);
        printf("    descriptor (%zu bytes) %s%s\n", d->descriptor_len, hex,
               shown < d->descriptor_len ? "..." : "");
    }
    printf("\n");
}

/* ------------------------------------------------------------------ JSON output */

static void json_string(const char *s) {
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)s; *p != '\0'; ++p) {
        if (*p == '"' || *p == '\\') {
            putchar('\\');
            putchar(*p);
        } else if (*p < 0x20u) {
            printf("\\u%04x", *p);
        } else {
            putchar(*p);
        }
    }
    putchar('"');
}

static void json_field_string(const char *key, const char *value) {
    printf("    \"%s\": ", key);
    if (value[0] == '\0') {
        fputs("null", stdout);
    } else {
        json_string(value);
    }
    fputs(",\n", stdout);
}

static void json_field_number(const char *key, int64_t value) {
    if (value < 0) {
        printf("    \"%s\": null,\n", key);
    } else {
        printf("    \"%s\": %lld,\n", key, (long long)value);
    }
}

static void print_json(const kf_hid_info *d, bool first) {
    fputs(first ? "  {\n" : ",\n  {\n", stdout);
    json_field_string("product", d->product);
    json_field_string("manufacturer", d->manufacturer);
    json_field_number("vendor_id", d->vendor_id);
    json_field_number("product_id", d->product_id);
    json_field_string("transport", d->transport);
    json_field_number("primary_usage_page", d->usage_page);
    json_field_number("primary_usage", d->usage);
    printf("    \"built_in\": %s,\n", d->built_in < 0 ? "null" : (d->built_in ? "true" : "false"));
    json_field_number("location_id", d->location_id);
    json_field_number("max_input_report_size", d->max_input_report);
    json_field_number("max_output_report_size", d->max_output_report);
    json_field_number("max_feature_report_size", d->max_feature_report);
    json_field_number("report_interval_us", d->report_interval_us);
    fputs("    \"usage_pairs\": [", stdout);
    const uint32_t stored = d->pair_count < KF_HID_MAX_PAIRS ? d->pair_count : KF_HID_MAX_PAIRS;
    for (uint32_t i = 0u; i < stored; ++i) {
        printf("%s[%u, %u]", i == 0u ? "" : ", ", d->pairs[i][0], d->pairs[i][1]);
    }
    fputs("],\n", stdout);
    static char hex[2u * KF_HID_MAX_DESCRIPTOR + 1u];
    const size_t kept =
        d->descriptor_len < KF_HID_MAX_DESCRIPTOR ? d->descriptor_len : KF_HID_MAX_DESCRIPTOR;
    (void)kf_tool_hex(hex, d->descriptor, kept);
    json_field_string("report_descriptor_hex", hex);
    printf("    \"report_descriptor_truncated\": %s,\n",
           kept < d->descriptor_len ? "true" : "false");
    json_field_string("io_class", d->io_class);
    printf("    \"registry_id\": %llu,\n", (unsigned long long)d->registry_id);
    printf("    \"registry_path\": ");
    json_string(d->path);
    fputs("\n  }", stdout);
}

/* ------------------------------------------------------------------ probe */

static int compare_u64(const void *a, const void *b) {
    const uint64_t x = *(const uint64_t *)a;
    const uint64_t y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

/*
 * The presence check the app can run on the main thread: one IOKit lookup, no HID manager,
 * nothing opened. The HID keys sit at the top level, where IOHIDDevice::matchPropertyTable
 * interprets them (inside IOPropertyMatch, DeviceUsagePage would be compared literally and
 * never match). Each run builds its own dictionary because IOServiceGetMatchingService
 * consumes it, so the timing includes that cost too.
 */
static int run_probe(const kf_tool_match *match, uint64_t expected_id) {
    uint64_t samples[PROBE_RUNS];
    uint64_t found_id = 0u;
    for (uint32_t run = 0u; run < PROBE_RUNS; ++run) {
        const uint64_t start = kf_time_now_ns();
        CFMutableDictionaryRef dict = IOServiceMatching(kIOHIDDeviceKey);
        if (dict == NULL || !kf_hid_match_fill(dict, match)) {
            fputs("hid-dump: out of memory\n", stderr);
            if (dict != NULL) {
                CFRelease(dict);
            }
            return EXIT_FAILURE;
        }
        const io_service_t service = IOServiceGetMatchingService(kIOMainPortDefault, dict);
        samples[run] = kf_time_now_ns() - start;
        found_id = 0u;
        if (service != MACH_PORT_NULL) {
            (void)IORegistryEntryGetRegistryEntryID(service, &found_id);
            IOObjectRelease(service);
        }
    }
    qsort(samples, PROBE_RUNS, sizeof samples[0], compare_u64);
    printf("probe: IOServiceGetMatchingService(IOServiceMatching(IOHIDDevice) + HID keys), %u "
           "runs\n",
           PROBE_RUNS);
    printf("  result       %s", found_id != 0u ? "found" : "not found");
    if (found_id != 0u) {
        printf(" registry-id 0x%llx (%s IOHIDManager)", (unsigned long long)found_id,
               found_id == expected_id ? "same as" : "DIFFERS from");
    }
    printf("\n  time         p50 %.1f us  p99 %.1f us  max %.1f us\n",
           (double)samples[PROBE_RUNS / 2u] / 1e3, (double)samples[(PROBE_RUNS * 99u) / 100u] / 1e3,
           (double)samples[PROBE_RUNS - 1u] / 1e3);
    if (found_id == 0u) {
        return EXIT_NO_MATCH;
    }
    return found_id == expected_id ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* ------------------------------------------------------------------ main */

int main(int argc, char **argv) {
    options opt;
    const int parsed = parse_options(argc, argv, &opt);
    if (parsed >= 0) {
        return parsed;
    }
    kf_time_init();

    IOHIDManagerRef manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    if (manager == NULL) {
        fputs("hid-dump: could not create the HID manager\n", stderr);
        return EXIT_FAILURE;
    }
    const CFArrayRef devices = kf_hid_copy_matching_devices(manager, &opt.match);
    const CFIndex count = devices != NULL ? CFArrayGetCount(devices) : 0;
    static kf_hid_info info; /* ~10 KB: keep it off the stack. */
    uint64_t first_id = 0u;

    if (opt.json) {
        fputs("[\n", stdout);
    }
    for (CFIndex i = 0; i < count; ++i) {
        const IOHIDDeviceRef device = (IOHIDDeviceRef)(uintptr_t)CFArrayGetValueAtIndex(devices, i);
        kf_hid_info_read(device, &info);
        if (i == 0) {
            first_id = info.registry_id;
        }
        if (opt.json) {
            print_json(&info, i == 0);
        } else {
            print_text(&info, (size_t)i + 1u, opt.verbose);
        }
    }
    if (opt.json) {
        fputs(count > 0 ? "\n]\n" : "]\n", stdout);
    } else {
        printf("%ld device%s\n", (long)count, count == 1 ? "" : "s");
    }

    int status = count > 0 ? EXIT_SUCCESS : EXIT_NO_MATCH;
    if (opt.probe) {
        if (count > 1) {
            fprintf(stderr, "hid-dump: --probe: %ld devices match; add flags to pick one\n",
                    (long)count);
            status = EXIT_USAGE;
        } else {
            printf("\n");
            status = run_probe(&opt.match, first_id); /* first_id is 0 when nothing matched. */
        }
    }

    if (devices != NULL) {
        CFRelease(devices);
    }
    CFRelease(manager);
    if (fflush(stdout) != 0 || ferror(stdout)) {
        return EXIT_FAILURE;
    }
    return status;
}
