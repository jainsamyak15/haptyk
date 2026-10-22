// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import <AppKit/AppKit.h>

#import "Application/KFAppContext.h"

NS_ASSUME_NONNULL_BEGIN

/// The calibration wizard (spec: velocity.c, "Calibration wizard math"): 3 s at rest, then
/// about 40 soft, 40 hard and 40 normal keystrokes; kf_calibration_compute() fits the result,
/// stored as calibration.v1. Without a usable motion sensor it says so and does nothing.
@interface KFCalibrationController : NSWindowController <NSWindowDelegate>
- (instancetype)initWithContext:(KFAppContext *)context NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithWindow:(nullable NSWindow *)window NS_UNAVAILABLE;
- (nullable instancetype)initWithCoder:(NSCoder *)coder NS_UNAVAILABLE;
@property (nonatomic, copy, nullable) void (^onClose)(void);
@end

NS_ASSUME_NONNULL_END
