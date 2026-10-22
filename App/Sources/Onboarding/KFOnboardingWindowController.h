// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import <AppKit/AppKit.h>

#import "Application/KFAppContext.h"

NS_ASSUME_NONNULL_BEGIN

/// First-run onboarding: welcome, Input Monitoring, the anonymous-stats choice, and a field to
/// try the sound. Polls the permission once a second only while its window is open.
@interface KFOnboardingWindowController : NSWindowController <NSWindowDelegate>
- (instancetype)initWithContext:(KFAppContext *)context NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithWindow:(nullable NSWindow *)window NS_UNAVAILABLE;
- (nullable instancetype)initWithCoder:(NSCoder *)coder NS_UNAVAILABLE;
@property (nonatomic, copy, nullable) void (^onClose)(void);
@end

NS_ASSUME_NONNULL_END
