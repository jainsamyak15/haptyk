// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import <Foundation/Foundation.h>

@class KFDayStats;
@class KFDiagnostics;
@class KFPreferences;
@class KFStatsController;

NS_ASSUME_NONNULL_BEGIN

/// Posted on the main queue when a new percentile table arrives.
extern NSNotificationName const KFTelemetryDidUpdateNotification;

/// The backend calls (spec: "Backend API"). Only with the stats opt-in: one daily POST of
/// complete past days, batching unsent ones, plus diagnostics. For everyone: the percentile
/// table, at most once a day, which sends nothing about the user. All HTTPS, 10 s timeout, off
/// the main thread, ephemeral (no cookies or cache on disk). Off entirely while the backend URL
/// in Info.plist (KFStatsBaseURL) is still a placeholder.
@interface KFTelemetry : NSObject

- (instancetype)initWithPreferences:(KFPreferences *)preferences
                              stats:(KFStatsController *)stats
                        diagnostics:(KFDiagnostics *)diagnostics NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

/// The configured backend, or nil while it is a placeholder.
@property (class, nonatomic, readonly, nullable) NSURL *baseURL;
/// Forces at p0..p100 for this Mac model (or all Macs), or nil without a fresh table.
@property (nonatomic, readonly, nullable) NSArray<NSNumber *> *percentileTable;

/// Schedules the daily job (NSBackgroundActivityScheduler) and loads the cached table.
- (void)start;
- (void)stop;

/// One day's upload payload: counts only (spec: "Telemetry"). Pure, for tests.
+ (NSDictionary<NSString *, id> *)payloadForDay:(KFDayStats *)day
                                      installId:(NSString *)installId
                                      forceMode:(NSString *)forceMode;

@end

NS_ASSUME_NONNULL_END
