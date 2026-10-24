// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "Calibration/KFCalibrationController.h"

#include <kf/kf.h>

#import "Power/KFPowerController.h"
#import "Preferences/KFPreferences.h"
#import "Support/KFUI.h"

enum { kKeysPerPhase = 40 }; /* A constant expression: it sizes the arrays below. */
static const uint64_t kRestNs = 3000000000ull;
static NSString *const kSentence = @"the quick brown fox jumps over the lazy dog";

typedef NS_ENUM(NSInteger, KFCalibrationPhase) {
    KFCalibrationIntro = 0,
    KFCalibrationRest,
    KFCalibrationSoft,
    KFCalibrationHard,
    KFCalibrationNormal,
    KFCalibrationDone,
};

@implementation KFCalibrationController {
    KFAppContext *_context;
    KFCalibrationPhase _phase;
    NSTextField *_instruction;
    NSTextField *_detail;
    NSProgressIndicator *_progress;
    NSButton *_primary;
    NSTimer *_timer;
    kf_calibration_sample _soft[kKeysPerPhase];
    kf_calibration_sample _hard[kKeysPerPhase];
    NSUInteger _softCount;
    NSUInteger _hardCount;
    NSUInteger _normalCount;
}

- (instancetype)initWithContext:(KFAppContext *)context {
    NSWindow *window =
        [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 500, 240)
                                    styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                                      backing:NSBackingStoreBuffered
                                        defer:YES];
    window.releasedWhenClosed = NO;
    window.title = @"Calibrate Keyfeel";
    if ((self = [super initWithWindow:window])) {
        _context = context;
        window.delegate = self;
        _instruction = KFHeading(@"");
        _detail = KFLabel(@"");
        _progress = [NSProgressIndicator new];
        _progress.indeterminate = NO;
        _progress.minValue = 0;
        _progress.maxValue = kKeysPerPhase;
        _progress.accessibilityLabel = @"Calibration progress";
        [_progress.widthAnchor constraintEqualToConstant:440].active = YES;
        _primary = KFPushButton(@"Start", self, @selector(primary:));
        _primary.keyEquivalent = @"\r";
        NSStackView *page = KFVStack(@[ _instruction, _detail, _progress, _primary ], 14.0);
        page.edgeInsets = NSEdgeInsetsMake(24, 24, 20, 24);
        page.translatesAutoresizingMaskIntoConstraints = NO;
        NSView *root = [NSView new];
        window.contentView = root;
        [root addSubview:page];
        [NSLayoutConstraint activateConstraints:@[
            [page.topAnchor constraintEqualToAnchor:root.topAnchor],
            [page.leadingAnchor constraintEqualToAnchor:root.leadingAnchor],
            [page.trailingAnchor constraintEqualToAnchor:root.trailingAnchor],
        ]];
        [self enter:KFCalibrationIntro];
        [window center];
    }
    return self;
}

- (void)enter:(KFCalibrationPhase)phase {
    _phase = phase;
    _progress.doubleValue = 0;
    _progress.hidden = phase == KFCalibrationIntro || phase == KFCalibrationDone;
    _primary.hidden = !(phase == KFCalibrationIntro || phase == KFCalibrationDone);
    const BOOL sensor = _context.power.forceStatus == KFForceStatusLive ||
                        _context.power.forceStatus == KFForceStatusNeedsCalibration;
    switch (phase) {
    case KFCalibrationIntro:
        _instruction.stringValue = @"Tune force detection to your typing";
        _detail.stringValue = sensor ? @"About a minute: rest your hands, then type a sentence "
                                       @"softly, hard, and normally."
                                     : @"This Mac's motion sensor is not available, so there is "
                                       @"nothing to calibrate. Every key plays at the same "
                                       @"strength.";
        _primary.title = sensor ? @"Start" : @"Close";
        break;
    case KFCalibrationRest:
        _instruction.stringValue = @"Hands off for 3 seconds";
        _detail.stringValue = @"Measuring how still the Mac is at rest.";
        kf_sensor_begin_noise_measurement(kRestNs);
        break;
    case KFCalibrationSoft:
        _instruction.stringValue = @"Type softly";
        _detail.stringValue =
            [NSString stringWithFormat:@"As lightly as still types: %@", kSentence];
        break;
    case KFCalibrationHard:
        _instruction.stringValue = @"Now type hard";
        _detail.stringValue =
            [NSString stringWithFormat:@"Firm, forceful keystrokes: %@", kSentence];
        break;
    case KFCalibrationNormal:
        _instruction.stringValue = @"Now type normally";
        _detail.stringValue =
            [NSString stringWithFormat:@"However you usually type: %@", kSentence];
        break;
    case KFCalibrationDone:
        [self finish];
        break;
    }
}

- (void)primary:(id)sender {
    (void)sender;
    if (_phase == KFCalibrationIntro &&
        (_context.power.forceStatus == KFForceStatusLive ||
         _context.power.forceStatus == KFForceStatusNeedsCalibration)) {
        _context.power.peaksClaimed = YES;
        kf_sensor_peak drop;
        while (kf_sensor_pop_peak(&drop)) {
        }
        _softCount = _hardCount = _normalCount = 0;
        [self enter:KFCalibrationRest];
        __weak KFCalibrationController *weakSelf = self;
        _timer = [NSTimer scheduledTimerWithTimeInterval:0.1
                                                 repeats:YES
                                                   block:^(NSTimer *timer) {
                                                       (void)timer;
                                                       [weakSelf tick];
                                                   }];
    } else {
        [self close];
    }
}

/// Every 0.1 s while the wizard runs: collect the keystrokes the sensor measured.
- (void)tick {
    if (_phase == KFCalibrationRest) {
        if (kf_sensor_noise_floor_g() >= 0.0f) {
            kf_sensor_peak drop;
            while (kf_sensor_pop_peak(&drop)) {
            }
            [self enter:KFCalibrationSoft];
        }
        return;
    }
    kf_sensor_peak peak;
    while (_phase < KFCalibrationDone && kf_sensor_pop_peak(&peak)) {
        const kf_calibration_sample sample = {peak.raw_g, peak.row};
        if (_phase == KFCalibrationSoft) {
            _soft[_softCount++] = sample;
            _progress.doubleValue = _softCount;
            if (_softCount == kKeysPerPhase) {
                [self enter:KFCalibrationHard];
            }
        } else if (_phase == KFCalibrationHard) {
            _hard[_hardCount++] = sample;
            _progress.doubleValue = _hardCount;
            if (_hardCount == kKeysPerPhase) {
                [self enter:KFCalibrationNormal];
            }
        } else if (_phase == KFCalibrationNormal) {
            _progress.doubleValue = ++_normalCount; /* Lets auto-adapt start from typical typing. */
            if (_normalCount == kKeysPerPhase) {
                [self enter:KFCalibrationDone];
            }
        }
    }
}

- (void)finish {
    [_timer invalidate];
    _timer = nil;
    _context.power.peaksClaimed = NO;
    kf_calibration cal = kf_calibration_default();
    const kf_status status = kf_calibration_compute(kf_sensor_noise_floor_g(), _soft, _softCount,
                                                    _hard, _hardCount, &cal);
    if (status != KF_OK) {
        _instruction.stringValue = @"Soft and hard typing were too alike";
        _detail.stringValue = @"Try again with lighter soft keystrokes and firmer hard ones.";
        _primary.title = @"Close";
        return;
    }
    NSMutableArray<NSNumber *> *gains = [NSMutableArray array];
    for (int r = 0; r < KF_ROW_COUNT; ++r) {
        [gains addObject:@(cal.row_gain[r])];
    }
    _context.preferences.calibration = @{
        @"noise_floor_g" : @(cal.noise_floor_g),
        @"soft_g" : @(cal.soft_g),
        @"slam_g" : @(cal.slam_g),
        @"row_gain" : gains,
    };
    _instruction.stringValue = @"Calibrated";
    _detail.stringValue = @"Keyfeel now matches your soft and hard keystrokes.";
    _primary.title = @"Done";
}

- (void)windowWillClose:(NSNotification *)notification {
    (void)notification;
    [_timer invalidate];
    _timer = nil;
    _context.power.peaksClaimed = NO;
    if (self.onClose != nil) {
        self.onClose();
    }
}

@end
