// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import <AppKit/AppKit.h>

#import "Application/KFAppDelegate.h"

int main(int argc, const char *argv[]) {
    (void)argc;
    (void)argv;
    @autoreleasepool {
        NSApplication *application = NSApplication.sharedApplication;
        KFAppDelegate *delegate = [KFAppDelegate new]; /* Strong ref for the app's lifetime. */
        application.delegate = delegate;
        [application run];
    }
    return EXIT_SUCCESS;
}
