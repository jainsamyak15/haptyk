// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import <AppKit/AppKit.h>

#import "Application/KFAppContext.h"

NS_ASSUME_NONNULL_BEGIN

/// Base of every settings tab: holds the context and refreshes when settings change.
@interface KFSettingsPane : NSViewController
- (instancetype)initWithContext:(KFAppContext *)context NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithNibName:(nullable NSNibName)nibNameOrNil
                         bundle:(nullable NSBundle *)nibBundleOrNil NS_UNAVAILABLE;
- (nullable instancetype)initWithCoder:(NSCoder *)coder NS_UNAVAILABLE;
@property (nonatomic, readonly) KFAppContext *context;
/// Called after any setting, pack or power change while the pane is loaded.
- (void)refresh;
/// Shows `alert` as a sheet on the settings window (modally if there is none).
- (void)present:(NSAlert *)alert then:(nullable void (^)(NSModalResponse response))done;
@end

@interface KFGeneralPane : KFSettingsPane
@end
@interface KFSoundPane : KFSettingsPane
@end
@interface KFPacksPane : KFSettingsPane
@end
@interface KFLabsPane : KFSettingsPane
@end
@interface KFStatsPane : KFSettingsPane
@end
@interface KFAboutPane : KFSettingsPane
@end

NS_ASSUME_NONNULL_END
