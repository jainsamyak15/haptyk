// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// Sparkle 2 updates (spec: "Updates, signing and release"). Without Xcode the app does not link
/// Sparkle: scripts/release.sh embeds Sparkle.framework, and this class loads it at run time and
/// drives SPUStandardUpdaterController. Inactive unless the framework is embedded and
/// Info.plist has a real SUFeedURL and SUPublicEDKey, so a debug build never checks for updates.
@interface KFUpdater : NSObject
@property (nonatomic, readonly) BOOL available;
/// Why updates are unavailable, for the menu's alert; nil when available.
@property (nonatomic, readonly, copy, nullable) NSString *unavailableReason;
- (void)start;
- (void)checkForUpdates;
@end

NS_ASSUME_NONNULL_END
