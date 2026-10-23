// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import <AppKit/AppKit.h>

#import "Application/KFAppContext.h"

NS_ASSUME_NONNULL_BEGIN

/// The status item: a left click opens the status panel (KFStatusPanelController), a right click
/// or Control-click the classic menu (spec: "App shell", Menu). Menu items are built once; values
/// refresh in menuWillOpen:, and live stats refresh every 0.5 s only while it is open.
@interface KFStatusMenuController : NSObject <NSMenuDelegate>
- (instancetype)initWithContext:(KFAppContext *)context NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
/// Debug builds: the status panel drawn to PNGs (KFStatusPanelController).
- (BOOL)renderPanelPreviewToDirectory:(NSURL *)directory;
@end

NS_ASSUME_NONNULL_END
