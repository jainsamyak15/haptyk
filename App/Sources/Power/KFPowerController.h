// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import <Foundation/Foundation.h>

#import "Power/KFPowerStateMachine.h"

@class KFPreferences;
@class KFPackLibrary;
@class KFStatsController;

NS_ASSUME_NONNULL_BEGIN

/// Posted on the main queue when the power state, force status or mic mute changes.
extern NSNotificationName const KFPowerStateDidChangeNotification;

/// What the menu's force line says (spec: "App shell", menu item 4).
typedef NS_ENUM(NSInteger, KFForceStatus) {
    KFForceStatusLive = 0,
    KFForceStatusExternalKeyboard,
    KFForceStatusUnavailable,
    KFForceStatusNeedsCalibration,
};

/// Shout Mode as it stands (Labs). It needs the tap in active mode, Accessibility and force
/// detection; without them the tap only listens and sounds play as usual.
typedef NS_ENUM(NSInteger, KFShoutStatus) {
    KFShoutStatusOff = 0,
    KFShoutStatusOn,                 ///< The tap runs in active mode.
    KFShoutStatusNeedsAccessibility, ///< Turned on, waiting for Accessibility.
    KFShoutStatusUnavailable,        ///< Turned on, but this Mac has no force detection.
    KFShoutStatusPaused,             ///< Turned on, but the tap is stopped (sounds off).
    KFShoutStatusTripped,            ///< Turned off after repeated tap timeouts; not yet seen.
};

/// Owns the power state machine and starts and stops the core: key tap, sensor and audio.
/// Idle costs nothing: audio and the sensor run only while typing, and no timer runs while
/// idle. Main thread.
@interface KFPowerController : NSObject

- (instancetype)initWithPreferences:(KFPreferences *)preferences
                            library:(KFPackLibrary *)library
                              stats:(KFStatsController *)stats NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@property (nonatomic, readonly) KFPowerState state;
@property (nonatomic, readonly) KFForceStatus forceStatus;
@property (nonatomic, readonly) BOOL microphoneMuted;
/// While YES (the calibration wizard runs) measured peaks are left for the wizard to read
/// instead of feeding the stats.
@property (nonatomic) BOOL peaksClaimed;
/// Last start failure for diagnostics, e.g. "audio error".
@property (nonatomic, readonly, copy, nullable) NSString *lastError;
@property (nonatomic, readonly) KFShoutStatus shoutStatus;

/// Launch: starts the key tap when sounds are on and Input Monitoring is granted.
- (void)start;
/// Quit: stops everything and flushes stats.
- (void)shutdown;
/// Re-checks Input Monitoring (menu opening, wake, onboarding) and moves state accordingly;
/// also switches Shout Mode's tap to active mode once Accessibility has been granted.
- (void)checkPermission;
/// Restarts the key tap in the mode Shout Mode needs now. Returns NO on failure.
- (BOOL)restartTap;
/// The user has seen that Shout Mode turned itself off (KFShoutStatusTripped clears).
- (void)acknowledgeShoutTrip;
/// Plays one key-down through the real path, for the onboarding test and pack preview.
- (void)playTestKeyWithVelocity:(float)velocity group:(uint8_t)group;

@end

NS_ASSUME_NONNULL_END
