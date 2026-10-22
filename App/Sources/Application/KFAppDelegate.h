// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import <AppKit/AppKit.h>

#import "Application/KFAppContext.h"

NS_ASSUME_NONNULL_BEGIN

/// Launch: owns the controllers and opens windows on demand. Module map: App/README.md.
@interface KFAppDelegate : NSObject <NSApplicationDelegate, KFAppActions>
@end

NS_ASSUME_NONNULL_END
