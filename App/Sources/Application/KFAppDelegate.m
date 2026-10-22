// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "Application/KFAppDelegate.h"

#import <ServiceManagement/ServiceManagement.h>

#include <kf/kf.h>

#import "Calibration/KFCalibrationController.h"
#import "Labs/KFForceMeterController.h"
#import "Menu/KFStatusMenuController.h"
#import "Onboarding/KFOnboardingWindowController.h"
#import "Packs/KFPackLibrary.h"
#import "Power/KFPowerController.h"
#import "Preferences/KFPreferences.h"
#import "Settings/KFSettingsWindowController.h"
#import "Stats/KFStatsController.h"
#import "Support/KFLog.h"
#import "Support/KFUpdater.h"
#import "Telemetry/KFDiagnostics.h"
#import "Telemetry/KFTelemetry.h"

@implementation KFAppDelegate {
    KFAppContext *_context;
    KFStatusMenuController *_menu;
    KFForceMeterController *_forceMeter;
    /* Windows exist only while open: each is dropped in its onClose. */
    KFSettingsWindowController *_settings;
    KFOnboardingWindowController *_onboarding;
    KFCalibrationController *_calibration;
    dispatch_source_t _sigterm;
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    (void)notification;
    kf_time_init();
    (void)kf_engine_init();

    _context = [KFAppContext new];
    _context.actions = self;
    _context.preferences = KFPreferences.shared;
    NSString *resources = NSBundle.mainBundle.resourcePath;
    _context.library = [[KFPackLibrary alloc]
        initWithBundledURL:resources != nil
                               ? [NSURL fileURLWithPath:[resources
                                                            stringByAppendingPathComponent:@"Packs"]
                                            isDirectory:YES]
                               : nil
                   userURL:KFApplicationSupportSubdirectory(@"Packs")];
    _context.stats =
        [[KFStatsController alloc] initWithDirectory:KFApplicationSupportSubdirectory(@"stats")];
    [_context.stats prune];
    _context.diagnostics =
        [[KFDiagnostics alloc] initWithDirectory:KFApplicationSupportSubdirectory(@"diagnostics")];
    [_context.diagnostics start];
    _context.telemetry = [[KFTelemetry alloc] initWithPreferences:_context.preferences
                                                            stats:_context.stats
                                                      diagnostics:_context.diagnostics];
    _context.updater = [KFUpdater new];
    _context.power = [[KFPowerController alloc] initWithPreferences:_context.preferences
                                                            library:_context.library
                                                              stats:_context.stats];
    _menu = [[KFStatusMenuController alloc] initWithContext:_context];
    [_context.power start];
    _forceMeter = [[KFForceMeterController alloc] initWithContext:_context];
    [_context.telemetry start];
    [_context.updater start];
#if defined(KF_DEBUG) && KF_DEBUG
    /* Debug builds only: `-KFSkipOnboarding YES` on the command line, for measuring the idle app
     * with no window open. Launch arguments are never saved. */
    const BOOL skipOnboarding =
        [NSUserDefaults.standardUserDefaults boolForKey:@"KFSkipOnboarding"];
#else
    const BOOL skipOnboarding = NO;
#endif
    if (!skipOnboarding && (!_context.preferences.onboardingDone ||
                            _context.power.state == KFPowerStateNoPermission)) {
        [self showOnboarding];
    }
    /* SIGTERM (logout, a process manager): quit cleanly, flushing stats and stopping audio. */
    signal(SIGTERM, SIG_IGN);
    _sigterm =
        dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGTERM, 0, dispatch_get_main_queue());
    dispatch_source_set_event_handler(_sigterm, ^{
        [NSApp terminate:nil];
    });
    dispatch_resume(_sigterm);
    os_log_info(KFLogApp(), "launched, %{public}@",
                [KFPowerStateMachine nameOfState:_context.power.state]);
}

- (void)applicationWillTerminate:(NSNotification *)notification {
    (void)notification;
    [_context.telemetry stop];
    [_context.diagnostics stop];
    [_context.power shutdown];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    (void)sender;
    return NO; /* A menu bar app: closing windows leaves the status item. */
}

#pragma mark - KFAppActions

- (void)showSettingsTab:(NSString *)identifier {
    if (_settings == nil) {
        _settings = [[KFSettingsWindowController alloc] initWithContext:_context];
        __weak KFAppDelegate *weakSelf = self;
        _settings.onClose = ^{
            KFAppDelegate *strongSelf = weakSelf;
            if (strongSelf != nil) {
                strongSelf->_settings = nil;
            }
        };
    }
    [_settings selectTab:identifier];
}

- (void)showOnboarding {
    if (_onboarding == nil) {
        _onboarding = [[KFOnboardingWindowController alloc] initWithContext:_context];
        __weak KFAppDelegate *weakSelf = self;
        _onboarding.onClose = ^{
            KFAppDelegate *strongSelf = weakSelf;
            if (strongSelf != nil) {
                strongSelf->_onboarding = nil;
            }
        };
    }
    [NSApp activateIgnoringOtherApps:YES];
    [_onboarding showWindow:nil];
}

- (void)showCalibration {
    if (_calibration == nil) {
        _calibration = [[KFCalibrationController alloc] initWithContext:_context];
        __weak KFAppDelegate *weakSelf = self;
        _calibration.onClose = ^{
            KFAppDelegate *strongSelf = weakSelf;
            if (strongSelf != nil) {
                strongSelf->_calibration = nil;
            }
        };
    }
    [NSApp activateIgnoringOtherApps:YES];
    [_calibration showWindow:nil];
}

- (void)checkForUpdates {
    if (_context.updater.available) {
        [_context.updater checkForUpdates];
        return;
    }
    NSAlert *alert = [NSAlert new];
    alert.messageText = @"Updates";
    alert.informativeText = _context.updater.unavailableReason ?: @"Updates are not available.";
    [NSApp activateIgnoringOtherApps:YES];
    [alert runModal];
}

- (void)sendFeedback {
    NSString *target = NSBundle.mainBundle.infoDictionary[@"KFFeedbackURL"];
    NSURL *url = target.length > 0 ? [NSURL URLWithString:target] : nil;
    if (url != nil) {
        [NSWorkspace.sharedWorkspace openURL:url];
    }
}

- (void)relaunch {
    NSWorkspaceOpenConfiguration *config = [NSWorkspaceOpenConfiguration configuration];
    config.createsNewApplicationInstance = YES;
    [NSWorkspace.sharedWorkspace openApplicationAtURL:NSBundle.mainBundle.bundleURL
                                        configuration:config
                                    completionHandler:^(NSRunningApplication *app, NSError *error) {
                                        (void)app;
                                        if (error == nil) {
                                            dispatch_async(dispatch_get_main_queue(), ^{
                                                [NSApp terminate:nil];
                                            });
                                        }
                                    }];
}

/// "Remove All Keyfeel Data" (spec: Safety rules, Clean removal): the app's own folders and
/// preferences, and the login item. Nothing else on the Mac is touched.
- (void)removeAllDataAndQuit {
    [_context.telemetry stop];
    [_context.power shutdown];
    (void)[SMAppService.mainAppService unregisterAndReturnError:NULL];
    NSFileManager *files = NSFileManager.defaultManager;
    NSString *bundle = KFBundleIdentifier();
    NSString *library =
        NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, YES).firstObject
            ?: [NSHomeDirectory() stringByAppendingPathComponent:@"Library"];
    NSArray<NSString *> *paths = @[
        KFApplicationSupportPath(),
        KFCachesPath(),
        [[library stringByAppendingPathComponent:@"HTTPStorages"]
            stringByAppendingPathComponent:bundle],
    ];
    for (NSString *path in paths) {
        [files removeItemAtPath:path error:NULL];
    }
    [_context.preferences removeAll];
    [NSUserDefaults.standardUserDefaults synchronize];
    [NSApp terminate:nil];
}

@end
