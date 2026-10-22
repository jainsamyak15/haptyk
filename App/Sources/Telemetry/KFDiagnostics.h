// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import <Foundation/Foundation.h>

@class KFAppContext;

NS_ASSUME_NONNULL_BEGIN

/// Crash and hang reports from MetricKit (zero runtime cost: macOS hands over earlier
/// sessions' diagnostics), kept 30 days, and the Export Diagnostics text. Neither ever holds
/// key data. Main thread.
@interface KFDiagnostics : NSObject

- (instancetype)initWithDirectory:(NSURL *)directory NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

/// Subscribes to MetricKit and drops reports older than 30 days.
- (void)start;
/// Ends the MetricKit subscription. Call while the object stays alive (it finishes later).
- (void)stop;
/// Saved reports not yet uploaded (sent only with the stats opt-in), as JSON objects.
- (NSArray<NSDictionary *> *)unsentReports;
- (void)markAllSent;
/// Count of reports kept on this Mac.
@property (nonatomic, readonly) NSUInteger reportCount;

/// The Export Diagnostics text: versions, sensor, audio, key tap and engine counters, settings.
- (NSString *)reportForContext:(KFAppContext *)context;

/// Stores one report; MetricKit's delegate path, public for tests.
- (void)saveReport:(NSData *)json;

@end

NS_ASSUME_NONNULL_END
