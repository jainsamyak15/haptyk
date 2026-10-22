// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// The spec's power states ("Power state machine").
typedef NS_ENUM(NSInteger, KFPowerState) {
    KFPowerStateDisabled = 0, ///< Sounds off: nothing runs.
    KFPowerStateNoPermission, ///< Input Monitoring missing: nothing runs, the badge shows.
    KFPowerStateIdle,         ///< Key tap only, waiting at no cost.
    KFPowerStateActive,       ///< Key tap, audio and (if present) the sensor.
};

typedef NS_ENUM(NSInteger, KFPowerEvent) {
    KFPowerEventEnable = 0,        ///< User turned sounds on.
    KFPowerEventDisable,           ///< User turned sounds off.
    KFPowerEventPermissionGranted, ///< Input Monitoring granted.
    KFPowerEventPermissionLost,    ///< Revoked, or the tap could not be created.
    KFPowerEventKeyDown,           ///< First key while idle.
    KFPowerEventIdleTimeout,       ///< No key for the idle timeout.
    KFPowerEventSleep,             ///< System sleep, screen lock or session switched out.
    KFPowerEventOutputChanged,     ///< Output device or its sample rate changed.
};

typedef NS_OPTIONS(NSUInteger, KFPowerAction) {
    KFPowerActionNone = 0,
    KFPowerActionStartTap = 1u << 0,
    KFPowerActionStopTap = 1u << 1,
    KFPowerActionStartAudio = 1u << 2, ///< Audio and sensor start together.
    KFPowerActionStopAudio = 1u << 3,  ///< Sensor stops first (resolving keys), then audio.
    KFPowerActionRestartAudio = 1u << 4,
    KFPowerActionArmIdleTimer = 1u << 5,
    KFPowerActionCancelIdleTimer = 1u << 6,
    KFPowerActionFlushStats = 1u << 7,
    KFPowerActionShowPermissionBadge = 1u << 8,
    KFPowerActionHidePermissionBadge = 1u << 9,
};

/// The transition table. Pure: no side effects, unit-tested.
@interface KFPowerStateMachine : NSObject
+ (KFPowerState)nextState:(KFPowerState)state
                    event:(KFPowerEvent)event
                  actions:(KFPowerAction *)actions;
+ (NSString *)nameOfState:(KFPowerState)state;
@end

NS_ASSUME_NONNULL_END
