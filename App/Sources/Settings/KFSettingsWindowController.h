// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import <AppKit/AppKit.h>

#import "Application/KFAppContext.h"

NS_ASSUME_NONNULL_BEGIN

/// The tabbed settings window. Created when opened, released when closed (`onClose`).
@interface KFSettingsWindowController : NSWindowController <NSWindowDelegate>
- (instancetype)initWithContext:(KFAppContext *)context NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithWindow:(nullable NSWindow *)window NS_UNAVAILABLE;
- (nullable instancetype)initWithCoder:(NSCoder *)coder NS_UNAVAILABLE;
/// general, sound, packs, labs, stats or about.
- (void)selectTab:(NSString *)identifier;
@property (nonatomic, copy, nullable) void (^onClose)(void);
@end

NS_ASSUME_NONNULL_END
