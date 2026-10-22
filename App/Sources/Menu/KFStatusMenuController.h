// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import <AppKit/AppKit.h>

#import "Application/KFAppContext.h"

NS_ASSUME_NONNULL_BEGIN

/// The status item and its menu (spec: "App shell", Menu). Items are built once; values
/// refresh in menuWillOpen:, and live stats refresh every 0.5 s only while the menu is open.
@interface KFStatusMenuController : NSObject <NSMenuDelegate>
- (instancetype)initWithContext:(KFAppContext *)context NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
@end

NS_ASSUME_NONNULL_END
