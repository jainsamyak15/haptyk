// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

/// The volume slider inside a menu item.
@interface KFVolumeMenuView : NSView
@property (nonatomic) float volume;
@property (nonatomic, copy, nullable) void (^onChange)(float volume);
- (instancetype)init;
@end

NS_ASSUME_NONNULL_END
