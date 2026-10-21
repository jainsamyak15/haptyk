// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/* Dev-tool helpers (Tools/common/kf_tool.h): parsing, formatting and device matching. */

#include <stdint.h>
#include <string.h>

#include "kf_test.h"
#include "kf_tool.h"

void test_tool_parse_numbers(void) {
    uint64_t v = 7u;
    KF_EXPECT(kf_tool_parse_u64("0", 10u, &v) && v == 0u);
    KF_EXPECT(kf_tool_parse_u64("1452", 0xffffu, &v) && v == 1452u);
    KF_EXPECT(kf_tool_parse_u64("0x05ac", 0xffffu, &v) && v == 0x05acu);
    KF_EXPECT(kf_tool_parse_u64("0XFF00", 0xffffu, &v) && v == 0xff00u);
    KF_EXPECT(kf_tool_parse_u64("0xffff", 0xffffu, &v) && v == 0xffffu);
    KF_EXPECT(kf_tool_parse_u64("18446744073709551615", UINT64_MAX, &v) && v == UINT64_MAX);

    v = 7u;
    const char *bad[] = {"", "0x", "-1", "+1", " 1", "1 ", "12a", "0x1g", "0b1", "1.5"};
    for (size_t i = 0u; i < sizeof bad / sizeof bad[0]; ++i) {
        KF_EXPECT(!kf_tool_parse_u64(bad[i], UINT64_MAX, &v));
    }
    KF_EXPECT(!kf_tool_parse_u64("0x10000", 0xffffu, &v));
    KF_EXPECT(!kf_tool_parse_u64("65536", 0xffffu, &v));
    KF_EXPECT(!kf_tool_parse_u64("18446744073709551616", UINT64_MAX, &v));
    KF_EXPECT(!kf_tool_parse_u64("5", 4u, &v));
    KF_EXPECT(!kf_tool_parse_u64(NULL, 10u, &v));
    KF_EXPECT(v == 7u); /* Untouched on failure. */
}

void test_tool_format_hex_and_decimal(void) {
    char hex[9];
    const uint8_t bytes[] = {0x00u, 0xabu, 0x0fu, 0xffu};
    KF_EXPECT(kf_tool_hex(hex, bytes, 4u) == 8u && strcmp(hex, "00ab0fff") == 0);
    KF_EXPECT(kf_tool_hex(hex, bytes, 0u) == 0u && hex[0] == '\0');

    char dec[KF_TOOL_U64_DIGITS + 1u];
    KF_EXPECT(kf_tool_u64_dec(dec, 0u) == 1u && strcmp(dec, "0") == 0);
    KF_EXPECT(kf_tool_u64_dec(dec, 1000u) == 4u && strcmp(dec, "1000") == 0);
    KF_EXPECT(kf_tool_u64_dec(dec, UINT64_MAX) == 20u && strcmp(dec, "18446744073709551615") == 0);
}

void test_tool_match_arguments(void) {
    char *argv[] = {"tool", "--vendor", "0x05ac", "--usage-page", "0xff00", "--usage",
                    "3",    "--name",   "accel",  "--transport",  "SPU",    "--json"};
    const int argc = (int)(sizeof argv / sizeof argv[0]);
    kf_tool_match m;
    kf_tool_match_init(&m);
    KF_EXPECT(kf_tool_match_is_empty(&m));
    int taken = 0;
    int i = 1;
    for (; i < argc; ++i) {
        const kf_tool_arg r = kf_tool_match_parse(&m, argc, argv, &i);
        if (r == KF_TOOL_ARG_NOT_MINE) {
            break;
        }
        KF_EXPECT(r == KF_TOOL_ARG_TAKEN);
        taken++;
    }
    KF_EXPECT(taken == 5 && i == 11 && strcmp(argv[i], "--json") == 0);
    KF_EXPECT(m.vendor_id == 0x05ac && m.product_id == -1 && m.usage_page == 0xff00);
    KF_EXPECT(m.usage == 3 && m.location_id == -1);
    KF_EXPECT(strcmp(m.product, "accel") == 0 && strcmp(m.transport, "SPU") == 0);
    KF_EXPECT(!kf_tool_match_is_empty(&m));

    char *missing[] = {"tool", "--usage"};
    char *empty[] = {"tool", "--name", ""};
    char *too_big[] = {"tool", "--usage-page", "0x10000"};
    struct {
        char **argv;
        int argc;
    } bad[] = {{missing, 2}, {empty, 3}, {too_big, 3}};
    for (size_t k = 0u; k < sizeof bad / sizeof bad[0]; ++k) {
        kf_tool_match_init(&m);
        int index = 1;
        KF_EXPECT(kf_tool_match_parse(&m, bad[k].argc, bad[k].argv, &index) == KF_TOOL_ARG_BAD);
        KF_EXPECT(index == 1 && kf_tool_match_is_empty(&m));
    }
}

static kf_hid_info make_accel(void) {
    kf_hid_info d;
    memset(&d, 0, sizeof d);
    d.vendor_id = 0x05ac;
    d.product_id = 0x8104;
    d.usage_page = 0xff00;
    d.usage = 3;
    d.location_id = 0;
    d.pair_count = 2u;
    d.pairs[0][0] = 0xff00u;
    d.pairs[0][1] = 3u;
    d.pairs[1][0] = 0x0001u;
    d.pairs[1][1] = 6u;
    (void)strcpy(d.product, "accel");
    (void)strcpy(d.transport, "SPU");
    return d;
}

void test_tool_match_info(void) {
    const kf_hid_info d = make_accel();
    kf_tool_match m;
    kf_tool_match_init(&m);
    KF_EXPECT(kf_tool_match_info(&m, &d)); /* Empty matches everything. */

    m.vendor_id = 0x05ac;
    m.usage_page = 0xff00;
    m.usage = 3;
    m.transport = "SPU";
    m.product = "accel";
    KF_EXPECT(kf_tool_match_info(&m, &d));

    /* Page and usage must hold for the same pair: ff00:0006 is not a pair. */
    m.usage = 6;
    KF_EXPECT(!kf_tool_match_info(&m, &d));
    m.usage_page = 0x0001;
    KF_EXPECT(kf_tool_match_info(&m, &d)); /* Any pair, not only the primary one. */
    m.usage_page = -1;
    KF_EXPECT(kf_tool_match_info(&m, &d));

    kf_tool_match_init(&m);
    m.product = "acc"; /* Exact, never a prefix. */
    KF_EXPECT(!kf_tool_match_info(&m, &d));
    kf_tool_match_init(&m);
    m.transport = "spu"; /* Case-sensitive, like IOKit. */
    KF_EXPECT(!kf_tool_match_info(&m, &d));
    kf_tool_match_init(&m);
    m.product_id = 0x8105;
    KF_EXPECT(!kf_tool_match_info(&m, &d));
    kf_tool_match_init(&m);
    m.location_id = 5;
    KF_EXPECT(!kf_tool_match_info(&m, &d));

    kf_hid_info absent = d; /* A device without a vendor ID never matches one. */
    absent.vendor_id = -1;
    kf_tool_match_init(&m);
    m.vendor_id = 0x05ac;
    KF_EXPECT(!kf_tool_match_info(&m, &absent));
}
