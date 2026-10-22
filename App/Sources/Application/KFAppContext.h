// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import <Foundation/Foundation.h>

@class KFPreferences;
@class KFPackLibrary;
@class KFPowerController;
@class KFStatsController;

NS_ASSUME_NONNULL_BEGIN

/// What windows and menus act on. They call back into the app through this protocol rather
/// than owning each other, so every window can be created on demand and released on close.
@protocol KFAppActions <NSObject>
- (void)showSettingsTab:(NSString *)identifier; ///< general, sound, packs, labs, stats, about
- (void)showOnboarding;
- (void)showCalibration;
- (void)checkForUpdates;
- (void)sendFeedback;
- (void)relaunch;
- (void)removeAllDataAndQuit;
@end

/// The long-lived controllers, created once at launch by KFAppDelegate.
@interface KFAppContext : NSObject
@property (nonatomic, strong) KFPreferences *preferences;
@property (nonatomic, strong) KFPackLibrary *library;
@property (nonatomic, strong) KFPowerController *power;
@property (nonatomic, strong) KFStatsController *stats;
@property (nonatomic, weak) id<KFAppActions> actions;
@end

NS_ASSUME_NONNULL_END
