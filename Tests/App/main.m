// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "KFAppTest.h"

#include <stdio.h>
#include <stdlib.h>

#include <kf/kf_time.h>

static int g_failures;
static NSMutableArray<NSURL *> *g_folders;

void KFTestFail(const char *file, int line, const char *expression) {
    g_failures++;
    fprintf(stderr, "    %s:%d: expected %s\n", file, line, expression);
}

NSURL *KFTestTemporaryFolder(void) {
    NSString *path = [NSString stringWithFormat:@"build/test-tmp/app-%@", NSUUID.UUID.UUIDString];
    NSURL *url = [NSURL fileURLWithPath:path isDirectory:YES].URLByStandardizingPath;
    [NSFileManager.defaultManager createDirectoryAtURL:url
                           withIntermediateDirectories:YES
                                            attributes:nil
                                                 error:NULL];
    [g_folders addObject:url];
    return url;
}

int main(void) {
    @autoreleasepool {
        kf_time_init();
        g_folders = [NSMutableArray array];
        const struct {
            const char *name;
            void (*run)(void);
        } tests[] = {
            {"power_state_machine", TestPowerStateMachine},
            {"pack_manifest_valid", TestPackManifestValid},
            {"pack_manifest_rejects_bad_input", TestPackManifestRejectsBadInput},
            {"pack_library_order_and_default", TestPackLibraryOrderAndDefault},
            {"labs_boom_and_meter", TestLabsBoomAndMeter},
            {"status_panel_pieces", TestStatusPanelPieces},
            {"preferences_defaults_and_clamping", TestPreferencesDefaultsAndClamping},
            {"stats_buckets_flush_and_reset", TestStatsBucketsFlushAndReset},
            {"badge_math", TestBadgeMath},
            {"telemetry_payload_and_safety", TestTelemetryPayloadAndSafety},
            {"diagnostics_reports", TestDiagnosticsReports},
        };
        int failed = 0;
        const size_t count = sizeof tests / sizeof tests[0];
        for (size_t i = 0; i < count; ++i) {
            const int before = g_failures;
            @autoreleasepool {
                tests[i].run();
            }
            const int ok = g_failures == before;
            failed += ok ? 0 : 1;
            printf("  %s  %s\n", ok ? "ok  " : "FAIL", tests[i].name);
        }
        for (NSURL *url in g_folders) {
            [NSFileManager.defaultManager removeItemAtURL:url error:NULL];
        }
        printf("\n%zu app tests, %d failed\n", count, failed);
        return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }
}
