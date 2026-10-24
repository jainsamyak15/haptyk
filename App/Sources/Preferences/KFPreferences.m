// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "Preferences/KFPreferences.h"

#import "Support/KFLog.h"

NSNotificationName const KFPreferencesDidChangeNotification = @"KFPreferencesDidChange";
NSString *const KFPreferencesKey = @"key";

static NSString *const kEnabled = @"enabled";
static NSString *const kVolume = @"volume";
static NSString *const kPackId = @"packId";
static NSString *const kReleaseSounds = @"releaseSounds";
static NSString *const kStereoWidth = @"stereoWidth";
static NSString *const kRepeatSounds = @"repeatSounds";
static NSString *const kMuteWhenMicActive = @"muteWhenMicActive";
static NSString *const kMuteExternalKeyboard = @"muteExternalKeyboard";
static NSString *const kIdleTimeout = @"idleTimeoutS";
static NSString *const kSensitivity = @"sensitivity";
static NSString *const kAutoAdapt = @"autoAdapt";
static NSString *const kForceSensing = @"forceSensingEnabled";
static NSString *const kCalibration = @"calibration.v1";
static NSString *const kBigEnter = @"labs.bigEnter";
static NSString *const kShoutMode = @"labs.shoutMode";
static NSString *const kForceMeter = @"labs.forceMeter";
static NSString *const kTelemetry = @"telemetryOptIn";
static NSString *const kOnboarding = @"onboardingDone";
static NSString *const kInstallId = @"installId";
static NSString *const kPanelStyle = @"panelStyle";

static float Clamp(float value, float lo, float hi) {
    if (!(value >= lo)) {
        return lo; /* Also maps NaN to lo. */
    }
    return value > hi ? hi : value;
}

@implementation KFPreferences {
    NSUserDefaults *_defaults;
}

+ (KFPreferences *)shared {
    static KFPreferences *shared;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        shared = [[KFPreferences alloc] initWithDefaults:NSUserDefaults.standardUserDefaults];
    });
    return shared;
}

- (instancetype)initWithDefaults:(NSUserDefaults *)defaults {
    if ((self = [super init])) {
        _defaults = defaults;
        [_defaults registerDefaults:@{
            kEnabled : @YES,
            kVolume : @0.7f,
            kReleaseSounds : @YES,
            kStereoWidth : @0.6f,
            kRepeatSounds : @NO,
            kMuteWhenMicActive : @YES,
            kMuteExternalKeyboard : @NO,
            kIdleTimeout : @30,
            kSensitivity : @0.0f,
            kAutoAdapt : @YES,
            kForceSensing : @NO,
            kBigEnter : @NO,
            kShoutMode : @NO,
            kForceMeter : @NO,
            kTelemetry : @NO,
            kOnboarding : @NO,
            kPanelStyle : @"graphite",
        }];
    }
    return self;
}

- (void)changed:(NSString *)key {
    NSDictionary *info = @{KFPreferencesKey : key};
    if (NSThread.isMainThread) {
        [NSNotificationCenter.defaultCenter postNotificationName:KFPreferencesDidChangeNotification
                                                          object:self
                                                        userInfo:info];
    } else {
        dispatch_async(dispatch_get_main_queue(), ^{
            [NSNotificationCenter.defaultCenter
                postNotificationName:KFPreferencesDidChangeNotification
                              object:self
                            userInfo:info];
        });
    }
}

- (void)setBool:(BOOL)value key:(NSString *)key {
    if ([_defaults boolForKey:key] != value) {
        [_defaults setBool:value forKey:key];
        [self changed:key];
    }
}

- (void)setFloat:(float)value key:(NSString *)key {
    if ([_defaults floatForKey:key] != value) {
        [_defaults setFloat:value forKey:key];
        [self changed:key];
    }
}

#pragma mark - Settings

- (NSString *)panelStyle {
    NSString *value = [_defaults stringForKey:kPanelStyle];
    return [value isEqualToString:@"glass"] ? @"glass" : @"graphite";
}
- (void)setPanelStyle:(NSString *)value {
    NSString *style = [value isEqualToString:@"glass"] ? @"glass" : @"graphite";
    if (![style isEqualToString:self.panelStyle]) {
        [_defaults setObject:style forKey:kPanelStyle];
        [self changed:kPanelStyle];
    }
}

- (BOOL)enabled {
    return [_defaults boolForKey:kEnabled];
}
- (void)setEnabled:(BOOL)value {
    [self setBool:value key:kEnabled];
}

- (float)volume {
    return Clamp([_defaults floatForKey:kVolume], 0.0f, 1.0f);
}
- (void)setVolume:(float)value {
    [self setFloat:Clamp(value, 0.0f, 1.0f) key:kVolume];
}

- (nullable NSString *)packId {
    id value = [_defaults objectForKey:kPackId];
    return [value isKindOfClass:NSString.class] ? value : nil;
}
- (void)setPackId:(nullable NSString *)value {
    if ([(value ?: @"") isEqualToString:(self.packId ?: @"")]) {
        return;
    }
    [_defaults setObject:value forKey:kPackId];
    [self changed:kPackId];
}

- (BOOL)releaseSounds {
    return [_defaults boolForKey:kReleaseSounds];
}
- (void)setReleaseSounds:(BOOL)value {
    [self setBool:value key:kReleaseSounds];
}

- (float)stereoWidth {
    return Clamp([_defaults floatForKey:kStereoWidth], 0.0f, 1.0f);
}
- (void)setStereoWidth:(float)value {
    [self setFloat:Clamp(value, 0.0f, 1.0f) key:kStereoWidth];
}

- (BOOL)repeatSounds {
    return [_defaults boolForKey:kRepeatSounds];
}
- (void)setRepeatSounds:(BOOL)value {
    [self setBool:value key:kRepeatSounds];
}

- (BOOL)muteWhenMicActive {
    return [_defaults boolForKey:kMuteWhenMicActive];
}
- (void)setMuteWhenMicActive:(BOOL)value {
    [self setBool:value key:kMuteWhenMicActive];
}

- (BOOL)muteExternalKeyboard {
    return [_defaults boolForKey:kMuteExternalKeyboard];
}
- (void)setMuteExternalKeyboard:(BOOL)value {
    [self setBool:value key:kMuteExternalKeyboard];
}

- (NSInteger)idleTimeoutS {
    const NSInteger value = [_defaults integerForKey:kIdleTimeout];
    return value < 5 ? 5 : (value > 600 ? 600 : value);
}
- (void)setIdleTimeoutS:(NSInteger)value {
    const NSInteger clamped = value < 5 ? 5 : (value > 600 ? 600 : value);
    if ([_defaults integerForKey:kIdleTimeout] != clamped) {
        [_defaults setInteger:clamped forKey:kIdleTimeout];
        [self changed:kIdleTimeout];
    }
}

- (float)sensitivity {
    return Clamp([_defaults floatForKey:kSensitivity], -1.0f, 1.0f);
}
- (void)setSensitivity:(float)value {
    [self setFloat:Clamp(value, -1.0f, 1.0f) key:kSensitivity];
}

- (BOOL)autoAdapt {
    return [_defaults boolForKey:kAutoAdapt];
}
- (void)setAutoAdapt:(BOOL)value {
    [self setBool:value key:kAutoAdapt];
}

/// Stored as a JSON string, as the spec says ("Save as JSON in user defaults").
- (nullable NSDictionary<NSString *, id> *)calibration {
    NSString *json = [_defaults stringForKey:kCalibration];
    NSData *data = [json dataUsingEncoding:NSUTF8StringEncoding];
    id value =
        data != nil ? [NSJSONSerialization JSONObjectWithData:data options:0 error:NULL] : nil;
    return [value isKindOfClass:NSDictionary.class] ? value : nil;
}
- (void)setCalibration:(nullable NSDictionary<NSString *, id> *)value {
    NSString *json = nil;
    if (value != nil) {
        NSDictionary *dict = value;
        NSData *data = [NSJSONSerialization isValidJSONObject:dict]
                           ? [NSJSONSerialization dataWithJSONObject:dict
                                                             options:NSJSONWritingSortedKeys
                                                               error:NULL]
                           : nil;
        json =
            data != nil ? [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] : nil;
    }
    [_defaults setObject:json forKey:kCalibration];
    [self changed:kCalibration];
}

- (BOOL)labsBigEnter {
    return [_defaults boolForKey:kBigEnter];
}
- (void)setLabsBigEnter:(BOOL)value {
    [self setBool:value key:kBigEnter];
}

- (BOOL)labsShoutMode {
    return [_defaults boolForKey:kShoutMode];
}
- (void)setLabsShoutMode:(BOOL)value {
    [self setBool:value key:kShoutMode];
}

- (BOOL)labsForceMeter {
    return [_defaults boolForKey:kForceMeter];
}
- (void)setLabsForceMeter:(BOOL)value {
    [self setBool:value key:kForceMeter];
}

- (BOOL)telemetryOptIn {
    return [_defaults boolForKey:kTelemetry];
}
- (void)setTelemetryOptIn:(BOOL)value {
    [self setBool:value key:kTelemetry];
}

- (BOOL)forceSensingEnabled {
    return [_defaults boolForKey:kForceSensing];
}
- (void)setForceSensingEnabled:(BOOL)value {
    [self setBool:value key:kForceSensing];
}

- (BOOL)onboardingDone {
    return [_defaults boolForKey:kOnboarding];
}
- (void)setOnboardingDone:(BOOL)value {
    [self setBool:value key:kOnboarding];
}

- (NSString *)installId {
    NSString *value = [_defaults stringForKey:kInstallId];
    if (value.length == 0) {
        value = NSUUID.UUID.UUIDString;
        [_defaults setObject:value forKey:kInstallId];
    }
    return value;
}

- (void)regenerateInstallId {
    [_defaults setObject:NSUUID.UUID.UUIDString forKey:kInstallId];
    [self changed:kInstallId];
}

- (void)removeAll {
    [_defaults removePersistentDomainForName:KFBundleIdentifier()];
    for (NSString *key in @[
             kEnabled, kVolume, kPackId, kReleaseSounds, kStereoWidth, kRepeatSounds,
             kMuteWhenMicActive, kMuteExternalKeyboard, kIdleTimeout, kSensitivity, kAutoAdapt,
             kForceSensing, kCalibration, kBigEnter, kShoutMode, kForceMeter, kTelemetry,
             kOnboarding, kInstallId
         ]) {
        [_defaults removeObjectForKey:key];
    }
}

@end
