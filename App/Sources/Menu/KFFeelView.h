// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "Menu/KFPanelControls.h"

NS_ASSUME_NONNULL_BEGIN

/// The panel's "Feel" card: the last keystroke's force tier as four rising bars, and typing
/// activity over the last minute as one bar per second. The activity works without force
/// detection, so the card is alive on every Mac. Drawn in one pass; updated only while the
/// panel is open.
@interface KFFeelView : KFCardView

/// The last keystroke: velocity 0..1, or negative when force is not measured; `status` is the
/// force line, e.g. "Force: Live".
- (void)setVelocity:(float)velocity status:(NSString *)status;

/// Key-downs per second, oldest first (KFFeelActivitySeconds values), and the last minute's total.
- (void)setActivity:(const uint32_t *)counts tapsPerMinute:(NSUInteger)tapsPerMinute;

@end

/// Seconds of activity the card shows.
extern const NSUInteger KFFeelActivitySeconds;

NS_ASSUME_NONNULL_END
