// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import <AppKit/AppKit.h>

#import "Application/KFAppContext.h"

NS_ASSUME_NONNULL_BEGIN

/// The status item's panel (ADR-0023): header with the on/off switch, attention banner, Feel card,
/// volume, pack chooser, stats, Labs and the menu's commands, in the "Graphite & Signal"
/// theme. A borderless, non-activating panel under the status item: it never steals focus from
/// the app being typed in, closes on Escape or a click elsewhere, and refreshes every 0.5 s
/// only while open (spec: Menu). Built on first open; nothing runs while it is closed.
@interface KFStatusPanelController : NSObject

- (instancetype)initWithContext:(KFAppContext *)context NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@property (nonatomic, readonly, getter=isShown) BOOL shown;

/// Opens under `button`, or closes if open.
- (void)toggleBelowButton:(NSStatusBarButton *)button;
- (void)close;

/// Debug builds: draws the panel in light and dark appearance to `directory`/panel-light.png and
/// panel-dark.png, for design review. NO if drawing failed.
- (BOOL)renderPreviewToDirectory:(NSURL *)directory;

@end

NS_ASSUME_NONNULL_END
