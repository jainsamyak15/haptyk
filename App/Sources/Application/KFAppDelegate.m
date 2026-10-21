// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "KFAppDelegate.h"

#include <kf/kf.h>

@implementation KFAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    (void)notification;
    kf_time_init();
    (void)kf_engine_init();
    // TODO(M3): create KFPowerController, KFStatusMenuController and, on first launch or when
    // Input Monitoring is missing, KFOnboardingWindowController. Spec: "App shell".
}

- (void)applicationWillTerminate:(NSNotification *)notification {
    (void)notification;
    // TODO(M3): stop the keytap, sensor and engine through KFPowerController.
}

@end
