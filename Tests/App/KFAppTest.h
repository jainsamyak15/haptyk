// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import <Foundation/Foundation.h>

/// Minimal test harness for the app layer (ADR-0004: XCTest needs Xcode, absent here).
/// Each test is a function registered in main.m; KFExpect records failures with file and line.

void KFTestFail(const char *file, int line, const char *expression);

#define KFExpect(condition)                                                                        \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            KFTestFail(__FILE__, __LINE__, #condition);                                            \
        }                                                                                          \
    } while (0)

/// A fresh, empty temporary folder inside build/, removed when the process exits.
NSURL *KFTestTemporaryFolder(void);

void TestPowerStateMachine(void);
void TestPackManifestValid(void);
void TestPackManifestRejectsBadInput(void);
void TestPackLibraryOrderAndDefault(void);
void TestLabsBoomAndMeter(void);
void TestStatusPanelPieces(void);
void TestPreferencesDefaultsAndClamping(void);
void TestStatsBucketsFlushAndReset(void);
void TestBadgeMath(void);
void TestTelemetryPayloadAndSafety(void);
void TestDiagnosticsReports(void);
