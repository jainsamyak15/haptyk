// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "Telemetry/KFTelemetry.h"

#include <sys/sysctl.h>

#include <kf/kf_sensor.h>

#import "Preferences/KFPreferences.h"
#import "Stats/KFStatsController.h"
#import "Support/KFLog.h"
#import "Telemetry/KFDiagnostics.h"

NSNotificationName const KFTelemetryDidUpdateNotification = @"KFTelemetryDidUpdate";

static const NSTimeInterval kTimeout = 10.0;
static const NSTimeInterval kTableFreshness = 24.0 * 3600;
static const NSUInteger kMaxBatchDays = 30;
static NSString *const kLastSentKey = @"telemetry.lastSentDay";

static NSString *Sysctl(const char *name) {
    char value[256] = "";
    size_t size = sizeof value;
    return sysctlbyname(name, value, &size, NULL, 0) == 0 ? @(value) : @"";
}

@implementation KFTelemetry {
    KFPreferences *_preferences;
    KFStatsController *_stats;
    KFDiagnostics *_diagnostics;
    NSURLSession *_session;
    NSBackgroundActivityScheduler *_scheduler;
    NSArray<NSNumber *> *_table;
}

- (instancetype)initWithPreferences:(KFPreferences *)preferences
                              stats:(KFStatsController *)stats
                        diagnostics:(KFDiagnostics *)diagnostics {
    if ((self = [super init])) {
        _preferences = preferences;
        _stats = stats;
        _diagnostics = diagnostics;
        NSURLSessionConfiguration *config = NSURLSessionConfiguration.ephemeralSessionConfiguration;
        config.timeoutIntervalForRequest = kTimeout;
        config.timeoutIntervalForResource = kTimeout;
        config.HTTPCookieStorage = nil;
        config.URLCache = nil;
        _session = [NSURLSession sessionWithConfiguration:config];
    }
    return self;
}

+ (nullable NSURL *)baseURL {
    NSString *text = NSBundle.mainBundle.infoDictionary[@"KFStatsBaseURL"];
    NSURL *url = text.length > 0 ? [NSURL URLWithString:text] : nil;
    if (url == nil || ![url.scheme isEqualToString:@"https"] ||
        [url.host hasSuffix:@"example.com"]) {
        return nil; /* A placeholder: nothing ever goes out. */
    }
    return url;
}

- (nullable NSArray<NSNumber *> *)percentileTable {
    return _table;
}

#pragma mark - Scheduling

- (void)start {
    [self loadCachedTable];
    if (KFTelemetry.baseURL == nil) {
        return;
    }
    _scheduler = [[NSBackgroundActivityScheduler alloc]
        initWithIdentifier:[KFBundleIdentifier() stringByAppendingString:@".daily"]];
    _scheduler.repeats = YES;
    _scheduler.interval = 24.0 * 3600;
    _scheduler.tolerance = 6.0 * 3600; /* macOS picks an efficient time. */
    _scheduler.qualityOfService = NSQualityOfServiceUtility;
    __weak KFTelemetry *weakSelf = self;
    [_scheduler scheduleWithBlock:^(NSBackgroundActivityCompletionHandler completion) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [weakSelf runDailyJob:^{
                completion(NSBackgroundActivityResultFinished);
            }];
        });
    }];
    [self runDailyJob:nil]; /* Unsent days are retried at the next launch. */
}

- (void)stop {
    [_scheduler invalidate];
    _scheduler = nil;
    [_session invalidateAndCancel];
}

- (void)runDailyJob:(nullable dispatch_block_t)done {
    dispatch_group_t group = dispatch_group_create();
    [self refreshTableInGroup:group];
    if (_preferences.telemetryOptIn) {
        [self uploadInGroup:group];
    }
    dispatch_group_notify(group, dispatch_get_main_queue(), ^{
        if (done != nil) {
            done();
        }
    });
}

#pragma mark - Percentile table

- (NSString *)cachePath {
    return [KFCachesPath() stringByAppendingPathComponent:@"percentiles.json"];
}

- (BOOL)acceptTable:(id)json {
    NSArray *values = [json isKindOfClass:NSDictionary.class] ? json[@"percentiles"] : nil;
    if (![values isKindOfClass:NSArray.class] || values.count < 2) {
        return NO;
    }
    for (id v in values) {
        if (![v isKindOfClass:NSNumber.class]) {
            return NO;
        }
    }
    _table = values;
    return YES;
}

- (void)loadCachedTable {
    NSDictionary *attributes = [NSFileManager.defaultManager attributesOfItemAtPath:self.cachePath
                                                                              error:NULL];
    NSDate *modified = attributes.fileModificationDate;
    NSData *data = [NSData dataWithContentsOfFile:self.cachePath];
    if (modified == nil || data == nil || -modified.timeIntervalSinceNow > kTableFreshness) {
        return; /* Stale or none: with no network the line stays hidden. */
    }
    id json = [NSJSONSerialization JSONObjectWithData:data options:0 error:NULL];
    if ([self acceptTable:json]) {
        [NSNotificationCenter.defaultCenter postNotificationName:KFTelemetryDidUpdateNotification
                                                          object:self];
    }
}

- (void)refreshTableInGroup:(dispatch_group_t)group {
    NSURL *base = KFTelemetry.baseURL;
    if (base == nil || _table != nil) {
        return;
    }
    NSURL *endpoint = [base URLByAppendingPathComponent:@"v1/stats/percentiles"];
    NSURLComponents *components = endpoint != nil
                                      ? [NSURLComponents componentsWithURL:(NSURL *_Nonnull)endpoint
                                                   resolvingAgainstBaseURL:NO]
                                      : nil;
    components.queryItems = @[ [NSURLQueryItem queryItemWithName:@"model"
                                                           value:Sysctl("hw.model")] ];
    NSURL *url = components.URL;
    if (url == nil) {
        return;
    }
    dispatch_group_enter(group);
    [[_session dataTaskWithURL:url
             completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
                 (void)error;
                 const NSInteger status = [(NSHTTPURLResponse *)response statusCode];
                 id json = status == 200 && data != nil
                               ? [NSJSONSerialization JSONObjectWithData:data options:0 error:NULL]
                               : nil;
                 dispatch_async(dispatch_get_main_queue(), ^{
                     if ([self acceptTable:json]) {
                         [NSFileManager.defaultManager createDirectoryAtPath:KFCachesPath()
                                                 withIntermediateDirectories:YES
                                                                  attributes:nil
                                                                       error:NULL];
                         [data writeToFile:self.cachePath atomically:YES];
                         [NSNotificationCenter.defaultCenter
                             postNotificationName:KFTelemetryDidUpdateNotification
                                           object:self];
                     }
                     dispatch_group_leave(group);
                 });
             }] resume];
}

#pragma mark - Upload (opt-in only)

+ (NSDictionary<NSString *, id> *)payloadForDay:(KFDayStats *)day
                                      installId:(NSString *)installId
                                      forceMode:(NSString *)forceMode {
    NSDictionary *info = NSBundle.mainBundle.infoDictionary;
    return @{
        @"install_id" : installId,
        @"app_version" : info[@"CFBundleShortVersionString"] ?: @"0",
        @"mac_model" : Sysctl("hw.model"),
        @"macos" : Sysctl("kern.osproductversion"),
        @"day" : day.day,
        @"keys" : @(day.keys),
        @"force_hist" : day.forceHistogram,
        @"force_mode" : forceMode,
    };
}

- (void)post:(id)body
          to:(NSString *)path
       group:(dispatch_group_t)group
        then:(void (^)(BOOL ok))then {
    NSURL *base = KFTelemetry.baseURL;
    NSData *json =
        base != nil ? [NSJSONSerialization dataWithJSONObject:body options:0 error:NULL] : nil;
    if (base == nil || json == nil) {
        then(NO);
        return;
    }
    NSMutableURLRequest *request = [NSMutableURLRequest
        requestWithURL:(NSURL *_Nonnull)[base URLByAppendingPathComponent:path]];
    request.HTTPMethod = @"POST";
    request.HTTPBody = json;
    [request setValue:@"application/json" forHTTPHeaderField:@"Content-Type"];
    dispatch_group_enter(group);
    [[_session dataTaskWithRequest:request
                 completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
                     (void)data;
                     (void)error;
                     const NSInteger status = [(NSHTTPURLResponse *)response statusCode];
                     dispatch_async(dispatch_get_main_queue(), ^{
                         then(status >= 200 && status < 300);
                         dispatch_group_leave(group);
                     });
                 }] resume];
}

- (void)uploadInGroup:(dispatch_group_t)group {
    [_stats flush];
    NSString *today = _stats.today.day;
    NSString *lastSent = [NSUserDefaults.standardUserDefaults stringForKey:kLastSentKey] ?: @"";
    NSMutableArray<NSDictionary *> *batch = [NSMutableArray array];
    NSString *newest = nil;
    NSString *mode = kf_sensor_probe() ? @"live" : @"fallback";
    for (KFDayStats *day in [[_stats recentDays:kMaxBatchDays] reverseObjectEnumerator]) {
        /* Complete days only, oldest first, each once. */
        if ([day.day compare:today] == NSOrderedAscending &&
            [day.day compare:lastSent] == NSOrderedDescending && day.keys > 0) {
            [batch addObject:[KFTelemetry payloadForDay:day
                                              installId:_preferences.installId
                                              forceMode:mode]];
            newest = day.day;
        }
    }
    if (batch.count > 0) {
        [self post:batch
                to:@"v1/telemetry/daily"
             group:group
              then:^(BOOL ok) {
                  if (ok && newest != nil) {
                      [NSUserDefaults.standardUserDefaults setObject:newest forKey:kLastSentKey];
                  }
              }];
    }
    NSArray<NSDictionary *> *reports = _diagnostics.unsentReports;
    if (reports.count > 0) {
        NSDictionary *body = @{
            @"app_version" : NSBundle.mainBundle.infoDictionary[@"CFBundleShortVersionString"]
                ?: @"0",
            @"mac_model" : Sysctl("hw.model"),
            @"reports" : reports,
        };
        [self post:@[ body ]
                to:@"v1/diagnostics"
             group:group
              then:^(BOOL ok) {
                  if (ok) {
                      [self->_diagnostics markAllSent];
                  }
              }];
    }
}

@end
