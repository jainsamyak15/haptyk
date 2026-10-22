// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "Stats/KFStatsController.h"

#include <math.h>
#include <stdlib.h>

#include <kf/kf_config.h>
#include <kf/kf_time.h>

#import "Support/KFLog.h"

static const uint64_t kNsPerMinute = 60ull * 1000000000ull;

@implementation KFDayStats

- (instancetype)init {
    if ((self = [super init])) {
        _day = @"";
        NSMutableArray *zeros = [NSMutableArray arrayWithCapacity:KF_STATS_FORCE_BUCKETS];
        for (NSUInteger i = 0; i < KF_STATS_FORCE_BUCKETS; ++i) {
            [zeros addObject:@0];
        }
        _forceHistogram = zeros;
    }
    return self;
}

- (NSDictionary<NSString *, id> *)dictionary {
    return @{
        @"day" : self.day,
        @"keys" : @(self.keys),
        @"active_minutes" : @(self.activeMinutes),
        @"peak_keys_per_minute" : @(self.peakKeysPerMinute),
        @"force_hist" : self.forceHistogram,
    };
}

+ (nullable instancetype)dayStatsWithDictionary:(NSDictionary<NSString *, id> *)dictionary {
    if (![dictionary isKindOfClass:NSDictionary.class]) {
        return nil;
    }
    id day = dictionary[@"day"];
    id keys = dictionary[@"keys"];
    id minutes = dictionary[@"active_minutes"];
    id peak = dictionary[@"peak_keys_per_minute"];
    id hist = dictionary[@"force_hist"];
    if (![day isKindOfClass:NSString.class] || ![keys isKindOfClass:NSNumber.class] ||
        ![minutes isKindOfClass:NSNumber.class] || ![peak isKindOfClass:NSNumber.class] ||
        ![hist isKindOfClass:NSArray.class] || [hist count] != KF_STATS_FORCE_BUCKETS) {
        return nil;
    }
    for (id value in hist) {
        if (![value isKindOfClass:NSNumber.class]) {
            return nil;
        }
    }
    KFDayStats *stats = [KFDayStats new];
    stats.day = day;
    stats.keys = [keys unsignedIntegerValue];
    stats.activeMinutes = [minutes unsignedIntegerValue];
    stats.peakKeysPerMinute = [peak unsignedIntegerValue];
    stats.forceHistogram = hist;
    return stats;
}

@end

@interface KFStatsController ()
@property (nonatomic, readwrite) KFDayStats *today;
@end

@implementation KFStatsController {
    NSURL *_directory;
    kf_live_stats *_live;
    uint64_t _flushedKeys;
    uint64_t _flushedMinute; /* Minutes up to here are already counted. */
    NSDateFormatter *_dayFormat;
}

- (instancetype)initWithDirectory:(NSURL *)directory {
    if ((self = [super init])) {
        _directory = directory;
        [NSFileManager.defaultManager createDirectoryAtURL:directory
                               withIntermediateDirectories:YES
                                                attributes:nil
                                                     error:NULL];
        _live = calloc(1, sizeof *_live);
        _dayFormat = [NSDateFormatter new];
        _dayFormat.locale = [NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];
        _dayFormat.dateFormat = @"yyyy-MM-dd";
        _flushedMinute = kf_time_now_ns() / kNsPerMinute;
        _today = [self loadDay:[_dayFormat stringFromDate:NSDate.date]];
    }
    return self;
}

- (void)dealloc {
    free(_live);
}

- (kf_live_stats *)live {
    return _live;
}

- (NSUInteger)tapsPerMinute {
    return kf_live_stats_last_minute(_live, kf_time_now_ns());
}

- (NSURL *)urlForDay:(NSString *)day {
    NSString *name = [day stringByAppendingString:@".json"];
    return [NSURL fileURLWithPath:[KFPath(_directory) stringByAppendingPathComponent:name]];
}

- (KFDayStats *)loadDay:(NSString *)day {
    NSData *data = [NSData dataWithContentsOfURL:[self urlForDay:day]];
    id json =
        data != nil ? [NSJSONSerialization JSONObjectWithData:data options:0 error:NULL] : nil;
    KFDayStats *stats = [json isKindOfClass:NSDictionary.class]
                            ? [KFDayStats dayStatsWithDictionary:(NSDictionary *_Nonnull)json]
                            : nil;
    if (stats == nil || ![stats.day isEqualToString:day]) {
        stats = [KFDayStats new];
        stats.day = day;
    }
    return stats;
}

- (void)write:(KFDayStats *)stats {
    NSData *data = [NSJSONSerialization dataWithJSONObject:stats.dictionary
                                                   options:NSJSONWritingSortedKeys
                                                     error:NULL];
    if (data == nil || ![data writeToURL:[self urlForDay:stats.day] atomically:YES]) {
        os_log_error(KFLogStats(), "could not write stats for %{public}@", stats.day);
    }
}

+ (NSUInteger)bucketForPeakG:(float)peakG {
    if (!(peakG > KF_STATS_FORCE_MIN_G)) {
        return 0;
    }
    const float span = logf(KF_STATS_FORCE_MAX_G) - logf(KF_STATS_FORCE_MIN_G);
    const float x =
        (logf(peakG) - logf(KF_STATS_FORCE_MIN_G)) / span * (float)KF_STATS_FORCE_BUCKETS;
    return x >= (float)KF_STATS_FORCE_BUCKETS ? KF_STATS_FORCE_BUCKETS - 1 : (NSUInteger)x;
}

- (void)notePeakG:(float)peakG {
    NSMutableArray<NSNumber *> *hist = [self.today.forceHistogram mutableCopy];
    const NSUInteger bucket = [KFStatsController bucketForPeakG:peakG];
    hist[bucket] = @(hist[bucket].unsignedIntegerValue + 1);
    self.today.forceHistogram = hist;
}

static void VisitMinute(uint32_t count, void *ctx) {
    KFDayStats *today = (__bridge KFDayStats *)ctx;
    today.activeMinutes += 1;
    today.peakKeysPerMinute = MAX(today.peakKeysPerMinute, (NSUInteger)count);
}

- (void)update {
    NSString *day = [_dayFormat stringFromDate:NSDate.date];
    if (![day isEqualToString:self.today.day]) {
        [self write:self.today]; /* Keys since the last flush stay with the day they ended. */
        self.today = [self loadDay:day];
    }
    const uint64_t keys = atomic_load_explicit(&_live->keys, memory_order_relaxed);
    self.today.keys += (NSUInteger)(keys - _flushedKeys);
    _flushedKeys = keys;
    /* Only complete minutes count, each exactly once. */
    const uint64_t minute = kf_time_now_ns() / kNsPerMinute;
    kf_live_stats_minutes(_live, _flushedMinute, minute, VisitMinute, (__bridge void *)self.today);
    _flushedMinute = minute > 0 ? minute - 1 : 0;
}

- (void)flush {
    [self update];
    [self write:self.today];
}

- (NSArray<KFDayStats *> *)recentDays:(NSUInteger)count {
    NSMutableArray<KFDayStats *> *days = [NSMutableArray array];
    NSCalendar *calendar = NSCalendar.currentCalendar;
    NSDate *date = NSDate.date;
    for (NSUInteger i = 0; i < count; ++i) {
        NSString *day = [_dayFormat stringFromDate:date];
        KFDayStats *stats = [day isEqualToString:self.today.day] ? self.today : [self loadDay:day];
        [days addObject:stats];
        date = [calendar dateByAddingUnit:NSCalendarUnitDay value:-1 toDate:date options:0];
    }
    return days;
}

- (NSArray<NSNumber *> *)histogramForLastDays:(NSUInteger)days {
    NSMutableArray<NSNumber *> *sum = [NSMutableArray arrayWithCapacity:KF_STATS_FORCE_BUCKETS];
    for (NSUInteger i = 0; i < KF_STATS_FORCE_BUCKETS; ++i) {
        [sum addObject:@0];
    }
    for (KFDayStats *day in [self recentDays:days]) {
        for (NSUInteger i = 0; i < KF_STATS_FORCE_BUCKETS; ++i) {
            sum[i] = @(sum[i].unsignedIntegerValue + day.forceHistogram[i].unsignedIntegerValue);
        }
    }
    return sum;
}

- (void)prune {
    NSDate *cutoff =
        [NSCalendar.currentCalendar dateByAddingUnit:NSCalendarUnitDay
                                               value:-(NSInteger)KF_STATS_RETENTION_DAYS
                                              toDate:NSDate.date
                                             options:0];
    NSString *oldest = [_dayFormat stringFromDate:cutoff];
    for (NSURL *url in [NSFileManager.defaultManager contentsOfDirectoryAtURL:_directory
                                                   includingPropertiesForKeys:nil
                                                                      options:0
                                                                        error:NULL]) {
        NSString *day = url.URLByDeletingPathExtension.lastPathComponent;
        if ([url.pathExtension isEqualToString:@"json"] && day.length == 10 &&
            [day compare:oldest] == NSOrderedAscending) {
            [NSFileManager.defaultManager removeItemAtURL:url error:NULL];
        }
    }
}

- (void)reset {
    for (NSURL *url in [NSFileManager.defaultManager contentsOfDirectoryAtURL:_directory
                                                   includingPropertiesForKeys:nil
                                                                      options:0
                                                                        error:NULL]) {
        if ([url.pathExtension isEqualToString:@"json"]) {
            [NSFileManager.defaultManager removeItemAtURL:url error:NULL];
        }
    }
    _flushedKeys = atomic_load_explicit(&_live->keys, memory_order_relaxed);
    _flushedMinute = kf_time_now_ns() / kNsPerMinute;
    KFDayStats *fresh = [KFDayStats new];
    fresh.day = [_dayFormat stringFromDate:NSDate.date];
    self.today = fresh;
}

@end
