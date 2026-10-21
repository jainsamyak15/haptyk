// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/**
 * @file kf_tool.h
 * @brief Portable helpers shared by the dev tools: strict number parsing, fast text
 *        formatting and HID device-matching arguments. Header-only and free of
 *        framework dependencies, so Tests/Core covers it on any host. Never shipped.
 */
#ifndef KF_TOOL_H
#define KF_TOOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/**
 * Parses an unsigned decimal or 0x-prefixed hexadecimal number no larger than `max`.
 * Rejects empty input, signs, whitespace, trailing characters and overflow. `out` is
 * written only on success.
 */
static inline bool kf_tool_parse_u64(const char *text, uint64_t max, uint64_t *out) {
    if (text == NULL || out == NULL || *text == '\0') {
        return false;
    }
    uint64_t base = 10u;
    const char *p = text;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        base = 16u;
        p += 2;
        if (*p == '\0') {
            return false;
        }
    }
    uint64_t value = 0u;
    for (; *p != '\0'; ++p) {
        uint64_t digit;
        if (*p >= '0' && *p <= '9') {
            digit = (uint64_t)(*p - '0');
        } else if (base == 16u && *p >= 'a' && *p <= 'f') {
            digit = (uint64_t)(*p - 'a') + 10u;
        } else if (base == 16u && *p >= 'A' && *p <= 'F') {
            digit = (uint64_t)(*p - 'A') + 10u;
        } else {
            return false;
        }
        if (digit > max || value > (max - digit) / base) {
            return false;
        }
        value = value * base + digit;
    }
    *out = value;
    return true;
}

/** Writes `n` bytes as 2n lowercase hex digits plus a NUL. `dst` holds 2n + 1. Returns 2n. */
static inline size_t kf_tool_hex(char *dst, const uint8_t *src, size_t n) {
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0u; i < n; ++i) {
        dst[2u * i] = digits[src[i] >> 4u];
        dst[2u * i + 1u] = digits[src[i] & 0x0fu];
    }
    dst[2u * n] = '\0';
    return 2u * n;
}

/** Longest decimal uint64_t, without the NUL. */
#define KF_TOOL_U64_DIGITS 20u

/** Writes `value` in decimal plus a NUL. `dst` holds KF_TOOL_U64_DIGITS + 1. Returns the length. */
static inline size_t kf_tool_u64_dec(char *dst, uint64_t value) {
    char reversed[KF_TOOL_U64_DIGITS];
    size_t n = 0u;
    do {
        reversed[n++] = (char)('0' + (int)(value % 10u));
        value /= 10u;
    } while (value != 0u);
    for (size_t i = 0u; i < n; ++i) {
        dst[i] = reversed[n - 1u - i];
    }
    dst[n] = '\0';
    return n;
}

/** HID matching criteria from the command line. -1 or NULL means "any". */
typedef struct kf_tool_match {
    int64_t vendor_id;
    int64_t product_id;
    int64_t usage_page; /**< Matched against every DeviceUsagePairs entry. */
    int64_t usage;
    int64_t location_id;
    const char *transport; /**< Exact, for example "SPU" or "USB". */
    const char *product;   /**< Exact product string, for example "accel". */
} kf_tool_match;

#define KF_TOOL_MATCH_USAGE                                                    \
    "  --vendor ID        vendor ID (decimal or 0x hex)\n"                     \
    "  --product ID       product ID\n"                                        \
    "  --usage-page P     usage page, matched against every usage pair\n"      \
    "  --usage U          usage, matched against every usage pair\n"           \
    "  --location L       location ID\n"                                       \
    "  --transport NAME   transport, exact (SPU, USB, Bluetooth, FIFO, ...)\n" \
    "  --name PRODUCT     product string, exact (accel, gyro, ...)\n"

static inline void kf_tool_match_init(kf_tool_match *match) {
    match->vendor_id = -1;
    match->product_id = -1;
    match->usage_page = -1;
    match->usage = -1;
    match->location_id = -1;
    match->transport = NULL;
    match->product = NULL;
}

static inline bool kf_tool_match_is_empty(const kf_tool_match *match) {
    return match->vendor_id < 0 && match->product_id < 0 && match->usage_page < 0 &&
           match->usage < 0 && match->location_id < 0 && match->transport == NULL &&
           match->product == NULL;
}

typedef enum kf_tool_arg {
    KF_TOOL_ARG_NOT_MINE = 0, /**< Not a matching flag; the caller handles it. */
    KF_TOOL_ARG_TAKEN,        /**< Consumed the flag and its value. */
    KF_TOOL_ARG_BAD,          /**< A matching flag with a missing or invalid value. */
} kf_tool_arg;

/**
 * Consumes argv[*index] and its value when it is a matching flag, advancing *index past
 * the value. On KF_TOOL_ARG_BAD, *index points at the offending flag.
 */
static inline kf_tool_arg kf_tool_match_parse(kf_tool_match *match, int argc, char *const *argv,
                                              int *index) {
    static const struct {
        const char *flag;
        uint64_t max;
    } k_numbers[] = {
        {"--vendor", 0xffffffffu}, {"--product", 0xffffffffu},  {"--usage-page", 0xffffu},
        {"--usage", 0xffffu},      {"--location", 0xffffffffu},
    };
    int64_t *const fields[] = {&match->vendor_id, &match->product_id, &match->usage_page,
                               &match->usage, &match->location_id};
    _Static_assert(sizeof fields / sizeof fields[0] == sizeof k_numbers / sizeof k_numbers[0],
                   "one field per numeric flag");

    const char *flag = argv[*index];
    const bool has_value = *index + 1 < argc && argv[*index + 1][0] != '\0';
    for (size_t i = 0u; i < sizeof k_numbers / sizeof k_numbers[0]; ++i) {
        if (strcmp(flag, k_numbers[i].flag) != 0) {
            continue;
        }
        uint64_t value;
        if (!has_value || !kf_tool_parse_u64(argv[*index + 1], k_numbers[i].max, &value)) {
            return KF_TOOL_ARG_BAD;
        }
        *fields[i] = (int64_t)value;
        *index += 1;
        return KF_TOOL_ARG_TAKEN;
    }
    const char **text = strcmp(flag, "--transport") == 0 ? &match->transport
                        : strcmp(flag, "--name") == 0    ? &match->product
                                                         : NULL;
    if (text == NULL) {
        return KF_TOOL_ARG_NOT_MINE;
    }
    if (!has_value) {
        return KF_TOOL_ARG_BAD;
    }
    *text = argv[*index + 1];
    *index += 1;
    return KF_TOOL_ARG_TAKEN;
}

#define KF_HID_MAX_PAIRS      32u
#define KF_HID_MAX_DESCRIPTOR 4096u

/** Everything the M0 tools report about one device. Numbers are -1 when the property is absent. */
typedef struct kf_hid_info {
    uint64_t registry_id;
    int64_t vendor_id;
    int64_t product_id;
    int64_t usage_page; /**< Primary usage page. */
    int64_t usage;      /**< Primary usage. */
    int64_t location_id;
    int64_t max_input_report;
    int64_t max_output_report;
    int64_t max_feature_report;
    int64_t report_interval_us;
    int built_in;        /**< 1 yes, 0 no, -1 absent. */
    uint32_t pair_count; /**< May exceed KF_HID_MAX_PAIRS; only the first ones are stored. */
    uint16_t pairs[KF_HID_MAX_PAIRS][2];
    size_t descriptor_len; /**< Full length; only the first KF_HID_MAX_DESCRIPTOR bytes are kept. */
    uint8_t descriptor[KF_HID_MAX_DESCRIPTOR];
    char product[128];
    char manufacturer[128];
    char transport[64];
    char io_class[128];
    char path[1024];
} kf_hid_info;

/**
 * True when `info` meets every criterion in `match`. IOKit matching silently ignores keys a
 * driver does not implement (IOHIDDevice ignores Product, for one), so the tools check every
 * criterion here too instead of trusting the kernel. Usage page and usage must hold for the
 * same usage pair.
 */
static inline bool kf_tool_match_info(const kf_tool_match *match, const kf_hid_info *info) {
    if ((match->vendor_id >= 0 && match->vendor_id != info->vendor_id) ||
        (match->product_id >= 0 && match->product_id != info->product_id) ||
        (match->location_id >= 0 && match->location_id != info->location_id) ||
        (match->transport != NULL && strcmp(match->transport, info->transport) != 0) ||
        (match->product != NULL && strcmp(match->product, info->product) != 0)) {
        return false;
    }
    if (match->usage_page < 0 && match->usage < 0) {
        return true;
    }
    const uint32_t stored =
        info->pair_count < KF_HID_MAX_PAIRS ? info->pair_count : KF_HID_MAX_PAIRS;
    for (uint32_t i = 0u; i < stored; ++i) {
        if ((match->usage_page < 0 || match->usage_page == info->pairs[i][0]) &&
            (match->usage < 0 || match->usage == info->pairs[i][1])) {
            return true;
        }
    }
    return false;
}

#endif /* KF_TOOL_H */
