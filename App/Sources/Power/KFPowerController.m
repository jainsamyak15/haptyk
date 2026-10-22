// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "Power/KFPowerController.h"

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>
#include <stdatomic.h>
#include <sys/sysctl.h>

#include <kf/kf.h>

#import "Packs/KFPackLibrary.h"
#import "Power/KFMicMonitor.h"
#import "Preferences/KFPreferences.h"
#import "Stats/KFStatsController.h"
#import "Support/KFLog.h"

NSNotificationName const KFPowerStateDidChangeNotification = @"KFPowerStateDidChange";

/* ------------------------------------------------------------------ keytap thread
 *
 * The key callback runs on the keytap thread and must return within 50 us: no Objective-C,
 * no locks, no allocation. It stores the key time, counts the key, and only for the first key
 * after idle hops to the main thread once (spec: "Implementation notes"). The one other hop is
 * as rare: once when Shout Mode has turned itself off after repeated tap timeouts. */

static _Atomic(uint64_t) g_last_key_ns;
static atomic_bool g_audio_active;
static atomic_bool g_wake_pending;
static kf_live_stats *g_live; /* Set before the tap starts; read-only afterwards. */

static void WakeOnMain(void *ctx);
static void ShoutTrippedOnMain(void *ctx);

static void OnKey(const kf_key_event *event, void *ctx) {
    atomic_store_explicit(&g_last_key_ns, event->t_ns, memory_order_relaxed);
    if (kf_keytap_take_shout_tripped()) { /* One atomic load unless it just happened. */
        dispatch_async_f(dispatch_get_main_queue(), ctx, ShoutTrippedOnMain);
    }
    if (!event->is_down) {
        return;
    }
    kf_live_stats_note_key(g_live, event->t_ns);
    if (!atomic_load_explicit(&g_audio_active, memory_order_relaxed) &&
        !atomic_exchange_explicit(&g_wake_pending, true, memory_order_relaxed)) {
        dispatch_async_f(dispatch_get_main_queue(), ctx, WakeOnMain);
    }
}

/* Engine thread-of-notification hop, already on the main queue. */
static void OnOutputChanged(void *ctx);

@interface KFPowerController ()
@property (nonatomic, readwrite) KFPowerState state;
@property (nonatomic, readwrite) KFForceStatus forceStatus;
@property (nonatomic, readwrite) BOOL microphoneMuted;
@property (nonatomic, readwrite, copy, nullable) NSString *lastError;
- (void)handle:(KFPowerEvent)event;
- (void)shoutTripped;
@end

static void WakeOnMain(void *ctx) {
    atomic_store_explicit(&g_wake_pending, false, memory_order_relaxed);
    [(__bridge KFPowerController *)ctx handle:KFPowerEventKeyDown];
}

static void ShoutTrippedOnMain(void *ctx) {
    [(__bridge KFPowerController *)ctx shoutTripped];
}

static void OnOutputChanged(void *ctx) {
    [(__bridge KFPowerController *)ctx handle:KFPowerEventOutputChanged];
}

@implementation KFPowerController {
    KFPreferences *_preferences;
    KFPackLibrary *_library;
    KFStatsController *_stats;
    KFMicMonitor *_mic;
    dispatch_source_t _idleTimer;
    dispatch_source_t _flushTimer;
    BOOL _tapActiveMode;
    BOOL _shoutTripped;
}

- (instancetype)initWithPreferences:(KFPreferences *)preferences
                            library:(KFPackLibrary *)library
                              stats:(KFStatsController *)stats {
    if ((self = [super init])) {
        _preferences = preferences;
        _library = library;
        _stats = stats;
        _state = KFPowerStateDisabled;
        _mic = [KFMicMonitor new];
        g_live = stats.live;
        __weak KFPowerController *weakSelf = self;
        _mic.onChange = ^(BOOL inUse) {
            [weakSelf micChanged:inUse];
        };
    }
    return self;
}

#pragma mark - Lifecycle

- (void)start {
    kf_engine_set_change_handler(OnOutputChanged, (__bridge void *)self);
    [self applyAllPreferences];
    [self loadCurrentPack];
    NSNotificationCenter *center = NSWorkspace.sharedWorkspace.notificationCenter;
    [center addObserver:self
               selector:@selector(systemWillSleep:)
                   name:NSWorkspaceWillSleepNotification
                 object:nil];
    [center addObserver:self
               selector:@selector(systemWillSleep:)
                   name:NSWorkspaceSessionDidResignActiveNotification
                 object:nil];
    [center addObserver:self
               selector:@selector(systemDidWake:)
                   name:NSWorkspaceDidWakeNotification
                 object:nil];
    [center addObserver:self
               selector:@selector(systemDidWake:)
                   name:NSWorkspaceSessionDidBecomeActiveNotification
                 object:nil];
    [NSDistributedNotificationCenter.defaultCenter addObserver:self
                                                      selector:@selector(systemWillSleep:)
                                                          name:@"com.apple.screenIsLocked"
                                                        object:nil];
    [NSDistributedNotificationCenter.defaultCenter addObserver:self
                                                      selector:@selector(systemDidWake:)
                                                          name:@"com.apple.screenIsUnlocked"
                                                        object:nil];
    [NSNotificationCenter.defaultCenter addObserver:self
                                           selector:@selector(preferencesChanged:)
                                               name:KFPreferencesDidChangeNotification
                                             object:_preferences];
    if (_preferences.enabled) {
        [self handle:KFPowerEventEnable];
    }
    [self updateForceStatus];
}

- (void)shutdown {
    [NSWorkspace.sharedWorkspace.notificationCenter removeObserver:self];
    [NSDistributedNotificationCenter.defaultCenter removeObserver:self];
    [NSNotificationCenter.defaultCenter removeObserver:self];
    [self handle:KFPowerEventDisable];
    [_mic stop];
    kf_engine_set_change_handler(NULL, NULL);
    [_stats flush];
}

#pragma mark - State machine

- (void)handle:(KFPowerEvent)event {
    KFPowerAction actions = KFPowerActionNone;
    const KFPowerState before = self.state;
    const KFPowerState after = [KFPowerStateMachine nextState:before event:event actions:&actions];
    self.state = after;
    [self perform:actions];
    if (after != before) {
        os_log_info(KFLogPower(), "%{public}@ -> %{public}@",
                    [KFPowerStateMachine nameOfState:before],
                    [KFPowerStateMachine nameOfState:after]);
    }
    [self updateForceStatus];
    [NSNotificationCenter.defaultCenter postNotificationName:KFPowerStateDidChangeNotification
                                                      object:self];
}

- (void)perform:(KFPowerAction)actions {
    if (actions & KFPowerActionCancelIdleTimer) {
        [self stopTimers];
    }
    if (actions & KFPowerActionStopAudio) {
        [self stopAudio];
    }
    if (actions & KFPowerActionStopTap) {
        kf_keytap_stop();
    }
    if (actions & KFPowerActionFlushStats) {
        [_stats flush];
    }
    if (actions & KFPowerActionStartTap) {
        [self startTap];
    }
    if (actions & KFPowerActionRestartAudio) {
        [self stopAudio];
        [self startAudio];
    }
    if (actions & KFPowerActionStartAudio) {
        [self startAudio];
    }
    if (actions & KFPowerActionArmIdleTimer) {
        [self startTimers];
    }
}

/// The tap filters events (active mode) only for Shout Mode, and only when it can act: force
/// detection exists and Accessibility is granted. Otherwise it only listens.
- (BOOL)wantsActiveTap {
    return _preferences.labsShoutMode && kf_sensor_probe() && AXIsProcessTrusted();
}

/// Opens the tap in the mode Shout Mode needs. If macOS refuses the active tap, sounds carry
/// on with a listen-only one.
- (kf_status)openTap {
    _tapActiveMode = [self wantsActiveTap];
    kf_status status = kf_keytap_start(OnKey, (__bridge void *)self, _tapActiveMode);
    if (status != KF_OK && _tapActiveMode) {
        os_log_info(KFLogPower(), "active tap refused; listening only");
        _tapActiveMode = NO;
        status = kf_keytap_start(OnKey, (__bridge void *)self, NO);
    }
    kf_keytap_set_shout(_tapActiveMode);
    if (status != KF_OK) {
        self.lastError = @(kf_status_string(status));
    }
    return status;
}

- (void)startTap {
    if (!kf_keytap_has_permission() || [self openTap] != KF_OK) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self handle:KFPowerEventPermissionLost];
        });
    }
}

- (BOOL)restartTap {
    if (self.state != KFPowerStateIdle && self.state != KFPowerStateActive) {
        return NO;
    }
    kf_keytap_stop();
    if ([self openTap] != KF_OK) {
        [self handle:KFPowerEventPermissionLost];
        return NO;
    }
    return YES;
}

/// Restarts the tap only when Shout Mode needs the other mode.
- (void)reconfigureTap {
    if ([self wantsActiveTap] != _tapActiveMode) {
        [self restartTap];
    }
    [NSNotificationCenter.defaultCenter postNotificationName:KFPowerStateDidChangeNotification
                                                      object:self];
}

- (KFShoutStatus)shoutStatus {
    if (!_preferences.labsShoutMode) {
        return _shoutTripped ? KFShoutStatusTripped : KFShoutStatusOff;
    }
    if (!kf_sensor_probe()) {
        return KFShoutStatusUnavailable;
    }
    if (!AXIsProcessTrusted()) {
        return KFShoutStatusNeedsAccessibility;
    }
    return _tapActiveMode && kf_keytap_is_running() ? KFShoutStatusOn : KFShoutStatusPaused;
}

/// macOS disabled the active tap too often: the core has already stopped adding Shift. Back
/// to a listen-only tap, and the menu tells the user (attention icon until they look).
- (void)shoutTripped {
    os_log_info(KFLogPower(), "Shout Mode off after %u tap timeouts in a minute",
                (unsigned)KF_SHOUT_MAX_TIMEOUTS);
    _shoutTripped = YES;
    _preferences.labsShoutMode = NO; /* Reconfigures the tap through preferencesChanged:. */
}

- (void)acknowledgeShoutTrip {
    if (_shoutTripped) {
        _shoutTripped = NO;
        [NSNotificationCenter.defaultCenter postNotificationName:KFPowerStateDidChangeNotification
                                                          object:self];
    }
}

- (void)checkPermission {
    const BOOL granted = kf_keytap_has_permission();
    if (granted && self.state == KFPowerStateNoPermission) {
        [self handle:KFPowerEventPermissionGranted];
    } else if (!granted && (self.state == KFPowerStateIdle || self.state == KFPowerStateActive)) {
        [self handle:KFPowerEventPermissionLost];
    } else if (_preferences.labsShoutMode &&
               (self.state == KFPowerStateIdle || self.state == KFPowerStateActive)) {
        [self reconfigureTap]; /* Accessibility granted (or revoked) since the tap started. */
    }
}

#pragma mark - Audio and sensor

- (void)loadCurrentPack {
    KFPackManifest *pack = [_library resolvePack:_preferences.packId];
    double rate = 0.0;
    if (pack == nil || kf_engine_default_output_rate(&rate) != KF_OK) {
        return;
    }
    if ([_library needsLoad:pack rate:rate]) {
        [_library loadPack:pack rate:rate completion:nil];
    }
}

- (void)startAudio {
    double rate = 0.0;
    if (kf_engine_default_output_rate(&rate) == KF_OK) {
        KFPackManifest *pack = [_library resolvePack:_preferences.packId];
        if (pack != nil && [_library needsLoad:pack rate:rate]) {
            /* The device changed while idle: decode at the new rate now (tens of ms), so the
             * first key never plays at the wrong pitch. */
            [_library loadPackNow:pack rate:rate error:NULL];
        }
    }
    const kf_status audio = kf_engine_start();
    if (audio != KF_OK) {
        self.lastError = @(kf_status_string(audio));
        os_log_error(KFLogPower(), "audio did not start: %{public}s", kf_status_string(audio));
    }
    atomic_store_explicit(&g_audio_active, audio == KF_OK, memory_order_relaxed);
    if (kf_sensor_probe()) {
        [self applyCalibration];
        (void)kf_sensor_start();
    }
}

- (void)stopAudio {
    kf_sensor_stop(); /* First: it resolves its pending keys onto the engine's queue. */
    kf_engine_stop();
    atomic_store_explicit(&g_audio_active, false, memory_order_relaxed);
    [self drainPeaks];
}

/// Peaks measured while typing feed today's force histogram, unless the wizard reads them.
- (void)drainPeaks {
    kf_sensor_peak peak;
    while (!self.peaksClaimed && kf_sensor_pop_peak(&peak)) {
        [_stats notePeakG:peak.raw_g];
    }
}

/// While typing: the idle check every half timeout, and the stats flush every 5 minutes.
/// Neither runs while idle.
- (void)startTimers {
    [self stopTimers];
    const uint64_t half = (uint64_t)_preferences.idleTimeoutS * NSEC_PER_SEC / 2u;
    __weak KFPowerController *weakSelf = self;
    _idleTimer =
        dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    dispatch_source_set_timer(_idleTimer, dispatch_time(DISPATCH_TIME_NOW, (int64_t)half), half,
                              half / 10u);
    dispatch_source_set_event_handler(_idleTimer, ^{
        [weakSelf checkIdle];
    });
    dispatch_resume(_idleTimer);
    const uint64_t flush = (uint64_t)KF_STATS_FLUSH_S * NSEC_PER_SEC;
    _flushTimer =
        dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    dispatch_source_set_timer(_flushTimer, dispatch_time(DISPATCH_TIME_NOW, (int64_t)flush), flush,
                              flush / 10u);
    dispatch_source_set_event_handler(_flushTimer, ^{
        [weakSelf flushWhileTyping];
    });
    dispatch_resume(_flushTimer);
}

- (void)stopTimers {
    if (_idleTimer != nil) {
        dispatch_source_cancel(_idleTimer);
        _idleTimer = nil;
    }
    if (_flushTimer != nil) {
        dispatch_source_cancel(_flushTimer);
        _flushTimer = nil;
    }
}

- (void)checkIdle {
    const uint64_t idle = (uint64_t)_preferences.idleTimeoutS * 1000000000ull;
    const uint64_t last = atomic_load_explicit(&g_last_key_ns, memory_order_relaxed);
    if (kf_time_now_ns() - last >= idle) {
        [self handle:KFPowerEventIdleTimeout];
    }
}

- (void)flushWhileTyping {
    [self drainPeaks];
    [_stats flush];
}

- (void)playTestKeyWithVelocity:(float)velocity group:(uint8_t)group {
    if (!kf_engine_is_running()) {
        if (self.state == KFPowerStateIdle) {
            atomic_store_explicit(&g_last_key_ns, kf_time_now_ns(), memory_order_relaxed);
            [self handle:KFPowerEventKeyDown];
        } else {
            [self startAudio];
        }
    }
    /* The main thread's own queue: never shared with the keytap or sensor threads. */
    const kf_key_event event = {kf_time_now_ns(), group, KF_ROW_HOME, 0, true, true};
    (void)kf_emit_keystroke(&event, velocity, KF_QUEUE_MAIN);
}

#pragma mark - System events

- (void)systemWillSleep:(NSNotification *)note {
    (void)note;
    [self handle:KFPowerEventSleep];
}

- (void)systemDidWake:(NSNotification *)note {
    (void)note;
    [self checkPermission];
}

- (void)micChanged:(BOOL)inUse {
    self.microphoneMuted = inUse && _preferences.muteWhenMicActive;
    kf_engine_set_muted(self.microphoneMuted); /* Mute only: unmuting is instant. */
    [NSNotificationCenter.defaultCenter postNotificationName:KFPowerStateDidChangeNotification
                                                      object:self];
}

#pragma mark - Preferences

/// Big Enter: the boom joins the pack only while the feature is on and force detection
/// exists to trigger it; otherwise it stays out of memory. The caller reloads the pack.
- (void)applyBigEnter {
    NSURL *boom = _preferences.labsBigEnter && kf_sensor_probe()
                      ? [NSBundle.mainBundle URLForResource:@"boom"
                                              withExtension:@"wav"
                                               subdirectory:@"Labs"]
                      : nil;
    _library.boomURL = boom;
    kf_engine_set_big_enter(boom != nil);
}

- (void)applyAllPreferences {
    [self applyBigEnter];
    kf_engine_set_volume(_preferences.volume);
    kf_engine_set_stereo_width(_preferences.stereoWidth);
    kf_keytap_set_repeat_sounds(_preferences.repeatSounds);
    kf_keytap_set_release_sounds(_preferences.releaseSounds);
    kf_keytap_set_mute_external(_preferences.muteExternalKeyboard);
    if (_preferences.muteWhenMicActive) {
        [_mic start];
    } else {
        [_mic stop];
        [self micChanged:NO];
    }
}

- (void)preferencesChanged:(NSNotification *)note {
    NSString *key = note.userInfo[KFPreferencesKey];
    if ([key isEqualToString:@"enabled"]) {
        [self handle:_preferences.enabled ? KFPowerEventEnable : KFPowerEventDisable];
    } else if ([key isEqualToString:@"packId"]) {
        [self loadCurrentPack];
    } else if ([key isEqualToString:@"idleTimeoutS"] && self.state == KFPowerStateActive) {
        [self startTimers];
    } else if ([key hasPrefix:@"calibration"] || [key isEqualToString:@"sensitivity"] ||
               [key isEqualToString:@"autoAdapt"]) {
        [self applyCalibration];
        [self updateForceStatus];
    } else if ([key isEqualToString:@"labs.shoutMode"]) {
        if (_preferences.labsShoutMode) {
            _shoutTripped = NO;
            [self requestAccessibilityIfUseful];
        }
        [self reconfigureTap];
    } else if ([key isEqualToString:@"labs.bigEnter"]) {
        [self applyBigEnter];
        [self loadCurrentPack]; /* Adds or drops the boom: a background decode, no gap. */
    } else if ([key isEqualToString:@"labs.forceMeter"]) {
        /* KFForceMeterController watches this itself. */
    } else {
        [self applyAllPreferences];
    }
}

/// Accessibility is asked for only when the user turns on Shout Mode and it could work on
/// this Mac (spec: "Onboarding"). macOS shows its own prompt; the app never touches TCC.
- (void)requestAccessibilityIfUseful {
    if (AXIsProcessTrusted() || !kf_sensor_probe()) {
        return;
    }
    NSDictionary *options = @{(__bridge NSString *)kAXTrustedCheckOptionPrompt : @YES};
    (void)AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef)options);
}

#pragma mark - Calibration

static NSString *HardwareModel(void) {
    char model[64] = "";
    size_t size = sizeof model;
    return sysctlbyname("hw.model", model, &size, NULL, 0) == 0 ? @(model) : @"";
}

/// User calibration (calibration.v1), else the model's default from
/// calibration-defaults.json, else the core's built-in default; then the sensitivity slider
/// shifts soft_g and slam_g together (2^-sensitivity, 0.5x to 2x).
- (kf_calibration)effectiveCalibration {
    kf_calibration cal = kf_calibration_default();
    NSDictionary *source = _preferences.calibration;
    if (source == nil) {
        NSURL *url = [NSBundle.mainBundle URLForResource:@"calibration-defaults"
                                           withExtension:@"json"];
        NSData *data = url != nil ? [NSData dataWithContentsOfURL:url] : nil;
        NSDictionary *json =
            data != nil ? [NSJSONSerialization JSONObjectWithData:data options:0 error:NULL] : nil;
        id models = [json isKindOfClass:NSDictionary.class] ? json[@"models"] : nil;
        source = [models isKindOfClass:NSDictionary.class] ? models[HardwareModel()] : nil;
    }
    if ([source isKindOfClass:NSDictionary.class]) {
        kf_calibration candidate = cal;
        candidate.noise_floor_g = [source[@"noise_floor_g"] floatValue];
        candidate.soft_g = [source[@"soft_g"] floatValue];
        candidate.slam_g = [source[@"slam_g"] floatValue];
        NSArray *gains = source[@"row_gain"];
        if ([gains isKindOfClass:NSArray.class] && gains.count == KF_ROW_COUNT) {
            for (int r = 0; r < KF_ROW_COUNT; ++r) {
                candidate.row_gain[r] = [gains[(NSUInteger)r] floatValue];
            }
        }
        if (kf_calibration_is_valid(&candidate)) {
            cal = candidate;
        }
    }
    const float shift = exp2f(-_preferences.sensitivity);
    cal.soft_g *= shift;
    cal.slam_g *= shift;
    cal.auto_adapt = _preferences.autoAdapt;
    return cal;
}

- (void)applyCalibration {
    const kf_calibration cal = [self effectiveCalibration];
    (void)kf_sensor_set_calibration(&cal);
}

- (void)updateForceStatus {
    KFForceStatus status = KFForceStatusLive;
    if (!kf_sensor_probe()) {
        status = KFForceStatusUnavailable;
    } else if (kf_keytap_external_keyboards() > 0) {
        status = KFForceStatusExternalKeyboard;
    } else if (_preferences.calibration == nil) {
        status = KFForceStatusNeedsCalibration;
    }
    self.forceStatus = status;
}

@end
