// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "Telemetry/KFDiagnostics.h"

#import <MetricKit/MetricKit.h>
#include <sys/sysctl.h>

#include <kf/kf.h>

#import "Application/KFAppContext.h"
#import "Packs/KFPackLibrary.h"
#import "Power/KFPowerController.h"
#import "Preferences/KFPreferences.h"
#import "Support/KFLog.h"

static const NSTimeInterval kKeepReports = 30.0 * 24 * 3600;
static NSString *const kSentSuffix = @".sent";

@interface KFDiagnostics () <MXMetricManagerSubscriber>
@end

@implementation KFDiagnostics {
    NSURL *_directory;
}

- (instancetype)initWithDirectory:(NSURL *)directory {
    if ((self = [super init])) {
        _directory = directory;
        [NSFileManager.defaultManager createDirectoryAtURL:directory
                               withIntermediateDirectories:YES
                                                attributes:nil
                                                     error:NULL];
    }
    return self;
}

- (void)start {
    [self prune];
    [MXMetricManager.sharedManager addSubscriber:self];
}

- (void)dealloc {
    [MXMetricManager.sharedManager removeSubscriber:self];
}

#pragma mark - MetricKit

- (void)didReceiveDiagnosticPayloads:(NSArray<MXDiagnosticPayload *> *)payloads {
    for (MXDiagnosticPayload *payload in payloads) {
        [self saveReport:payload.JSONRepresentation];
    }
}

#pragma mark - Storage

- (NSArray<NSURL *> *)files {
    return [[NSFileManager.defaultManager contentsOfDirectoryAtURL:_directory
                                        includingPropertiesForKeys:@[ NSURLCreationDateKey ]
                                                           options:0
                                                             error:NULL]
        sortedArrayUsingComparator:^NSComparisonResult(NSURL *a, NSURL *b) {
            return [KFPath(a).lastPathComponent compare:KFPath(b).lastPathComponent];
        }];
}

- (void)saveReport:(NSData *)json {
    NSString *name = [NSString stringWithFormat:@"%.0f-%@.json", NSDate.date.timeIntervalSince1970,
                                                [NSUUID.UUID.UUIDString substringToIndex:8]];
    NSURL *url = [NSURL fileURLWithPath:[KFPath(_directory) stringByAppendingPathComponent:name]];
    if (![json writeToURL:url atomically:YES]) {
        os_log_error(KFLogApp(), "could not keep a diagnostic report");
    }
}

- (void)prune {
    NSDate *cutoff = [NSDate dateWithTimeIntervalSinceNow:-kKeepReports];
    for (NSURL *url in self.files) {
        NSDate *created = nil;
        [url getResourceValue:&created forKey:NSURLCreationDateKey error:NULL];
        if (created != nil && [created compare:cutoff] == NSOrderedAscending) {
            [NSFileManager.defaultManager removeItemAtURL:url error:NULL];
        }
    }
}

- (NSUInteger)reportCount {
    NSUInteger count = 0;
    for (NSURL *url in self.files) {
        count += [url.pathExtension isEqualToString:@"json"] ? 1 : 0;
    }
    return count;
}

- (NSArray<NSDictionary *> *)unsentReports {
    NSMutableArray<NSDictionary *> *reports = [NSMutableArray array];
    NSSet<NSString *> *names = [NSSet setWithArray:[self.files valueForKey:@"lastPathComponent"]];
    for (NSURL *url in self.files) {
        if (![url.pathExtension isEqualToString:@"json"] ||
            [names containsObject:[KFPath(url).lastPathComponent
                                      stringByAppendingString:kSentSuffix]]) {
            continue;
        }
        NSData *data = [NSData dataWithContentsOfURL:url];
        id json =
            data != nil ? [NSJSONSerialization JSONObjectWithData:data options:0 error:NULL] : nil;
        if ([json isKindOfClass:NSDictionary.class]) {
            [reports addObject:json];
        }
    }
    return reports;
}

- (void)markAllSent {
    NSSet<NSString *> *names = [NSSet setWithArray:[self.files valueForKey:@"lastPathComponent"]];
    for (NSURL *url in self.files) {
        NSString *marker = [KFPath(url).lastPathComponent stringByAppendingString:kSentSuffix];
        if ([url.pathExtension isEqualToString:@"json"] && ![names containsObject:marker]) {
            [NSData.data
                writeToURL:[NSURL fileURLWithPath:[KFPath(_directory)
                                                      stringByAppendingPathComponent:marker]]
                atomically:YES];
        }
    }
}

#pragma mark - Export

static NSString *Sysctl(const char *name) {
    char value[256] = "";
    size_t size = sizeof value;
    return sysctlbyname(name, value, &size, NULL, 0) == 0 ? @(value) : @"?";
}

static NSString *SensorState(kf_sensor_state state) {
    switch (state) {
    case KF_SENSOR_ABSENT:
        return @"absent";
    case KF_SENSOR_STOPPED:
        return @"stopped";
    case KF_SENSOR_LIVE:
        return @"live";
    case KF_SENSOR_STALE:
        return @"stale";
    case KF_SENSOR_DENIED:
        return @"denied";
    }
    return @"?";
}

- (NSString *)reportForContext:(KFAppContext *)context {
    NSDictionary *info = NSBundle.mainBundle.infoDictionary;
    kf_engine_counters engine;
    kf_sensor_counters sensor;
    kf_keytap_counters keytap;
    kf_engine_get_counters(&engine);
    kf_sensor_get_counters(&sensor);
    kf_keytap_get_counters(&keytap);
    char device[128];
    kf_engine_output_device_name(device, sizeof device);
    KFPreferences *p = context.preferences;
    KFPackManifest *pack = context.library.loadedPack;
    NSMutableString *out = [NSMutableString string];
    [out appendString:@"Keyfeel diagnostics\n===================\n"];
    [out appendFormat:@"Generated: %@\n",
                      [NSISO8601DateFormatter
                          stringFromDate:NSDate.date
                                timeZone:NSTimeZone.localTimeZone
                           formatOptions:NSISO8601DateFormatWithInternetDateTime]];
    [out appendFormat:@"App: %@ (%@), core %s\n", info[@"CFBundleShortVersionString"],
                      info[@"CFBundleVersion"], KF_CORE_VERSION_STRING];
    [out appendFormat:@"Mac: %@, macOS %@ (%@)\n", Sysctl("hw.model"),
                      Sysctl("kern.osproductversion"), Sysctl("kern.osversion")];
    [out
        appendFormat:@"Power state: %@%@\n", [KFPowerStateMachine nameOfState:context.power.state],
                     context.power.lastError != nil
                         ? [NSString stringWithFormat:@" (last error: %@)", context.power.lastError]
                         : @""];
    [out appendFormat:
             @"\nSensor: %@; %.1f Hz measured; %llu samples, %llu reports dropped; keys %llu "
             @"measured, %llu at default\n",
             SensorState(kf_sensor_state_now(kf_time_now_ns())), sensor.sample_rate_hz,
             sensor.samples, sensor.dropped_reports, sensor.keys_resolved, sensor.keys_defaulted];
    [out appendFormat:@"Audio: %s; %s; %.0f Hz; %u-frame buffer granted; start to first callback "
                      @"%.1f ms\n",
                      device[0] != '\0' ? device : "no device",
                      kf_engine_is_running() ? "running" : "stopped", engine.sample_rate,
                      engine.granted_buffer_frames, (double)engine.start_latency_ns / 1e6];
    [out appendFormat:
             @"Engine: %llu callbacks; p50 %.0f us, p99 %.0f us, max %.1f us; %llu overruns; "
             @"requests %llu dropped, %llu stale; %llu voices stolen\n",
             engine.callbacks, (double)engine.callback_p50_ns / 1e3,
             (double)engine.callback_p99_ns / 1e3, (double)engine.max_callback_ns / 1e3,
             engine.callback_overruns, engine.requests_dropped, engine.requests_stale,
             engine.voices_stolen];
    [out appendFormat:@"Key tap: %s; %llu events, %llu tap timeouts, %llu repeats dropped; %u "
                      @"external keyboards\n",
                      kf_keytap_is_running() ? "running" : "stopped", keytap.events,
                      keytap.tap_timeouts, keytap.autorepeat_dropped,
                      kf_keytap_external_keyboards()];
    [out appendFormat:@"Pack: %@\n", pack != nil
                                         ? [NSString stringWithFormat:@"%@ %@ (%@)", pack.name,
                                                                      pack.version, pack.identifier]
                                         : @"none"];
    [out appendFormat:
             @"\nSettings: sounds %@, volume %.2f, stereo width %.2f, release %@, repeat %@, "
             @"mic mute %@, external mute %@, idle %ld s, sensitivity %.2f, auto-adapt %@, "
             @"calibrated %@, labs %@/%@/%@, stats sharing %@\n",
             p.enabled ? @"on" : @"off", (double)p.volume, (double)p.stereoWidth,
             p.releaseSounds ? @"on" : @"off", p.repeatSounds ? @"on" : @"off",
             p.muteWhenMicActive ? @"on" : @"off", p.muteExternalKeyboard ? @"on" : @"off",
             (long)p.idleTimeoutS, (double)p.sensitivity, p.autoAdapt ? @"on" : @"off",
             p.calibration != nil ? @"yes" : @"no", p.labsBigEnter ? @"on" : @"off",
             p.labsShoutMode ? @"on" : @"off", p.labsForceMeter ? @"on" : @"off",
             p.telemetryOptIn ? @"on" : @"off"];
    [out appendFormat:@"Crash and hang reports kept: %lu\n", (unsigned long)self.reportCount];
    [out appendString:
             @"\nNo key data, typed text, app names, window titles or URLs are included.\n"];
    return out;
}

@end
