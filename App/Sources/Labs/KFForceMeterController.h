// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import <Foundation/Foundation.h>

@class KFAppContext;

NS_ASSUME_NONNULL_BEGIN

/// Force Meter (Labs): a small floating HUD that shows each keystroke's force for
/// KF_FORCE_METER_SHOW_MS. Its refresh timer (every KF_FORCE_METER_POLL_MS) runs only while
/// the feature is on and the user is typing (power state ACTIVE); otherwise nothing runs and
/// the panel does not exist. It reads the core's atomic counters, never the key path, and
/// never takes focus or clicks. Main thread.
@interface KFForceMeterController : NSObject

- (instancetype)initWithContext:(KFAppContext *)context NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

/// Whether the refresh timer runs.
@property (nonatomic, readonly, getter=isRunning) BOOL running;

/// "72% · Hard", or "Not measured" when the force is unknown (negative or NaN).
+ (NSString *)textForVelocity:(float)velocity;
/// Soft, Medium, Hard or Slam: the recorded tier nearest to `velocity` (clamped to 0..1).
+ (NSString *)tierNameForVelocity:(float)velocity;

@end

NS_ASSUME_NONNULL_END
