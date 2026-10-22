// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, KFStatusIconState) {
    KFStatusIconOn = 0,
    KFStatusIconOff,       ///< Drawn dimmed.
    KFStatusIconAttention, ///< A dot: Input Monitoring is missing, or a Labs feature stopped.
};

/// Template (monochrome) status-bar images, drawn in code so they follow the menu bar's
/// appearance and need no assets.
@interface KFStatusIcon : NSObject
+ (NSImage *)imageForState:(KFStatusIconState)state;
@end

NS_ASSUME_NONNULL_END
