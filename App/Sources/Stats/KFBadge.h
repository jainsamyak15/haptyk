// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

/// Badges by force percentile (spec: "Percentile and badges").
typedef NS_ENUM(NSInteger, KFBadgeKind) {
    KFBadgeFeather = 0, ///< Below the 25th percentile.
    KFBadgeSteady,      ///< 25th to 60th.
    KFBadgeHammer,      ///< 60th to 90th.
    KFBadgeThunder,     ///< Top 10%.
};

/// Pure percentile and badge math, plus the shareable badge card. Main thread.
@interface KFBadge : NSObject

/// Keystrokes with a measured force needed before a badge (spec: 500).
@property (class, nonatomic, readonly) NSUInteger requiredKeystrokes;

/// Median force in g of a histogram with fixed log-spaced edges (KF_STATS_FORCE_*),
/// interpolated within its bucket; NAN when empty.
+ (double)medianForceOfHistogram:(NSArray<NSNumber *> *)histogram;

/// The share of users (0..100) whose force is below `force`, from a table of forces at each
/// percentile, ascending (101 values for p0..p100). NAN for an unusable table.
+ (double)percentileOfForce:(double)force table:(NSArray<NSNumber *> *)table;

+ (KFBadgeKind)badgeForPercentile:(double)percentile;
+ (NSString *)nameOfBadge:(KFBadgeKind)badge;

/// A 1200 x 630 PNG card: badge, percentile and pack name. Drawn locally; nothing is sent.
+ (nullable NSData *)cardWithBadge:(KFBadgeKind)badge
                        percentile:(double)percentile
                          packName:(NSString *)packName;

@end

NS_ASSUME_NONNULL_END
