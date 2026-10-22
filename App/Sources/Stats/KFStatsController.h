// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import <Foundation/Foundation.h>

#include "Stats/kf_live_stats.h"

NS_ASSUME_NONNULL_BEGIN

/// One day of local stats: counts only (spec: "Stats, telemetry and privacy").
@interface KFDayStats : NSObject
@property (nonatomic, copy) NSString *day; ///< yyyy-MM-dd, local time.
@property (nonatomic) NSUInteger keys;
@property (nonatomic) NSUInteger activeMinutes;
@property (nonatomic) NSUInteger peakKeysPerMinute;
/// KF_STATS_FORCE_BUCKETS counts of row-compensated peaks, fixed log-spaced edges.
@property (nonatomic, copy) NSArray<NSNumber *> *forceHistogram;
- (NSDictionary<NSString *, id> *)dictionary;
+ (nullable instancetype)dayStatsWithDictionary:(NSDictionary<NSString *, id> *)dictionary;
@end

/// Local stats in `directory/YYYY-MM-DD.json`, kept KF_STATS_RETENTION_DAYS days. The keytap
/// thread updates `live` with atomics; everything else runs on the main thread. Writes at most
/// every KF_STATS_FLUSH_S, and when typing stops or the app quits.
@interface KFStatsController : NSObject

- (instancetype)initWithDirectory:(NSURL *)directory NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

/// The counters the key callback updates. Valid for the controller's lifetime.
@property (nonatomic, readonly) kf_live_stats *live;
@property (nonatomic, readonly) KFDayStats *today;
/// Key-downs in the last 60 s.
@property (nonatomic, readonly) NSUInteger tapsPerMinute;

/// A measured keystroke's row-compensated peak (from kf_sensor_pop_peak).
- (void)notePeakG:(float)peakG;
/// Merges the live counters into today, in memory only (cheap; the menu calls it).
- (void)update;
/// update, then writes today's file.
- (void)flush;
/// Days with stats, newest first, at most `count`.
- (NSArray<KFDayStats *> *)recentDays:(NSUInteger)count;
/// Deletes files older than the retention period.
- (void)prune;
/// "Reset Stats": deletes every stats file and starts today again from zero.
- (void)reset;

/// Histogram bucket of a peak, 0..KF_STATS_FORCE_BUCKETS-1, clamped at both ends.
+ (NSUInteger)bucketForPeakG:(float)peakG;

@end

NS_ASSUME_NONNULL_END
