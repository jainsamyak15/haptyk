// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import <AppKit/AppKit.h>

@class KFPackManifest;

NS_ASSUME_NONNULL_BEGIN

/// The panel's pack chooser: filter chips (All, Linear, Tactile, Clicky, Other) over a list of
/// packs, each with its switch type, the current one marked. Arrow keys and VoiceOver work as
/// in any table. Choosing a row calls onChoose.
@interface KFPackListView : NSView

@property (nonatomic, copy) NSArray<KFPackManifest *> *packs;
@property (nonatomic, copy, nullable) NSString *currentIdentifier;
@property (nonatomic, copy, nullable) void (^onChoose)(KFPackManifest *pack);

/// Visible rows before the list scrolls.
@property (nonatomic) NSUInteger visibleRows;
/// On a glass tile: the list's card draws no surface of its own.
@property (nonatomic) BOOL glassBacked;

/// Rebuilds the rows (after packs, the current pack or the filter change).
- (void)reload;
/// Scrolls the current pack into view.
- (void)revealCurrent;

/// Switch types a filter chip shows: nil for All.
+ (nullable NSArray<NSString *> *)typesForFilter:(NSString *)filter;

@end

NS_ASSUME_NONNULL_END
