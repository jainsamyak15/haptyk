// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "Power/KFPowerStateMachine.h"

@implementation KFPowerStateMachine

/// What leaving `state` for good needs, so DISABLED and NO_PERMISSION stop exactly what runs.
static KFPowerAction StopEverything(KFPowerState state) {
    switch (state) {
    case KFPowerStateActive:
        return KFPowerActionStopAudio | KFPowerActionCancelIdleTimer | KFPowerActionStopTap |
               KFPowerActionFlushStats;
    case KFPowerStateIdle:
        return KFPowerActionStopTap;
    case KFPowerStateDisabled:
    case KFPowerStateNoPermission:
        return KFPowerActionNone;
    }
    return KFPowerActionNone;
}

+ (KFPowerState)nextState:(KFPowerState)state
                    event:(KFPowerEvent)event
                  actions:(KFPowerAction *)actions {
    KFPowerAction a = KFPowerActionNone;
    KFPowerState next = state;
    switch (event) {
    case KFPowerEventDisable:
        a = StopEverything(state) |
            (state == KFPowerStateNoPermission ? KFPowerActionHidePermissionBadge : 0);
        next = KFPowerStateDisabled;
        break;
    case KFPowerEventEnable:
        if (state == KFPowerStateDisabled) {
            a = KFPowerActionStartTap; /* The controller reports PermissionLost if it fails. */
            next = KFPowerStateIdle;
        }
        break;
    case KFPowerEventPermissionLost:
        if (state != KFPowerStateDisabled) {
            a = StopEverything(state) | KFPowerActionShowPermissionBadge;
            next = KFPowerStateNoPermission;
        }
        break;
    case KFPowerEventPermissionGranted:
        if (state == KFPowerStateNoPermission) {
            a = KFPowerActionHidePermissionBadge | KFPowerActionStartTap;
            next = KFPowerStateIdle;
        }
        break;
    case KFPowerEventKeyDown:
        if (state == KFPowerStateIdle) {
            /* This key plays at the default velocity once audio runs. */
            a = KFPowerActionStartAudio | KFPowerActionArmIdleTimer;
            next = KFPowerStateActive;
        }
        break;
    case KFPowerEventIdleTimeout:
    case KFPowerEventSleep:
        if (state == KFPowerStateActive) {
            a = KFPowerActionStopAudio | KFPowerActionCancelIdleTimer | KFPowerActionFlushStats;
            next = KFPowerStateIdle;
        }
        break;
    case KFPowerEventOutputChanged:
        if (state == KFPowerStateActive) {
            a = KFPowerActionRestartAudio;
        }
        break;
    }
    if (actions != NULL) {
        *actions = a;
    }
    return next;
}

+ (NSString *)nameOfState:(KFPowerState)state {
    switch (state) {
    case KFPowerStateDisabled:
        return @"disabled";
    case KFPowerStateNoPermission:
        return @"no permission";
    case KFPowerStateIdle:
        return @"idle";
    case KFPowerStateActive:
        return @"active";
    }
    return @"unknown";
}

@end
