// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "Onboarding/KFOnboardingWindowController.h"

#include <kf/kf.h>

#import "Power/KFPowerController.h"
#import "Preferences/KFPreferences.h"
#import "Support/KFUI.h"

static NSString *const kInputMonitoringSettings =
    @"x-apple.systempreferences:com.apple.preference.security?Privacy_ListenEvent";

typedef NS_ENUM(NSInteger, KFOnboardingStep) {
    KFStepWelcome = 0,
    KFStepPermission,
    KFStepStats,
    KFStepTry,
    KFStepCount,
};

@implementation KFOnboardingWindowController {
    KFAppContext *_context;
    KFOnboardingStep _step;
    NSView *_body;
    NSButton *_back;
    NSButton *_next;
    NSTimer *_poll;
    NSTextField *_permissionStatus;
    NSButton *_relaunch;
    NSTextField *_tier;
}

- (instancetype)initWithContext:(KFAppContext *)context {
    NSWindow *window =
        [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 520, 340)
                                    styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                                      backing:NSBackingStoreBuffered
                                        defer:YES];
    window.releasedWhenClosed = NO;
    window.title = @"Welcome to Keyfeel";
    window.contentView = [NSView new];
    if ((self = [super initWithWindow:window])) {
        _context = context;
        window.delegate = self;
        _back = KFPushButton(@"Back", self, @selector(back:));
        _next = KFPushButton(@"Continue", self, @selector(next:));
        _next.keyEquivalent = @"\r";
        [self show:KFStepWelcome];
        [window center];
    }
    return self;
}

- (NSView *)viewForStep:(KFOnboardingStep)step {
    switch (step) {
    case KFStepWelcome:
        return KFVStack(
            @[
                KFHeading(@"Your MacBook, with the sound of a mechanical keyboard"),
                KFLabel(@"Keyfeel plays a switch sound for every key you press, from the menu bar. "
                        @"On MacBooks with a usable motion sensor it also plays softer or harder "
                        @"sounds for softer or harder keystrokes."),
                KFSecondaryLabel(@"Keyfeel never records what you type: keys pick a sound and are "
                                 @"forgotten at once."),
            ],
            12.0);
    case KFStepPermission: {
        _permissionStatus = KFLabel(@"");
        _relaunch = KFPushButton(@"Relaunch Keyfeel", self, @selector(relaunch:));
        _relaunch.hidden = YES;
        return KFVStack(
            @[
                KFHeading(@"Allow Input Monitoring"),
                KFLabel(@"macOS asks before an app may notice key presses. Keyfeel listens only to "
                        @"know when to play a sound, and passes every key through untouched."),
                KFHStack(
                    @[
                        KFPushButton(@"Ask Me", self, @selector(requestPermission:)),
                        KFPushButton(@"Open System Settings", self, @selector(openSettings:)),
                    ],
                    8.0),
                _permissionStatus,
                _relaunch,
            ],
            12.0);
    }
    case KFStepStats: {
        NSButton *optIn =
            KFCheckbox(@"Share anonymous daily counts", self, @selector(toggleStats:));
        optIn.state = _context.preferences.telemetryOptIn;
        return KFVStack(
            @[
                KFHeading(@"Anonymous stats"),
                KFLabel(@"If you like, Keyfeel can send how many keys you pressed each day and how "
                        @"hard, once a day. Never which keys, text, apps, windows or URLs."),
                optIn,
                KFSecondaryLabel(@"Off unless you turn it on. You can change it any time in "
                                 @"Settings > Stats."),
            ],
            12.0);
    }
    case KFStepTry: {
        NSTextField *field = [NSTextField textFieldWithString:@""];
        field.placeholderString = @"Type here to hear Keyfeel";
        field.accessibilityLabel = @"Try typing";
        [field.widthAnchor constraintEqualToConstant:440].active = YES;
        _tier = KFSecondaryLabel(@"");
        return KFVStack(
            @[
                KFHeading(@"Try it"),
                field,
                _tier,
                KFSecondaryLabel(@"Change the sound and volume from the menu bar icon."),
            ],
            12.0);
    }
    case KFStepCount:
        break;
    }
    return [NSView new];
}

- (void)show:(KFOnboardingStep)step {
    _step = step;
    [_body removeFromSuperview];
    NSView *content = [self viewForStep:step];
    NSStackView *buttons = KFHStack(@[ _back, _next ], 8.0);
    _back.hidden = step == KFStepWelcome;
    _next.title = step == KFStepTry ? @"Done" : @"Continue";
    NSStackView *page = KFVStack(@[ content, buttons ], 24.0);
    page.edgeInsets = NSEdgeInsetsMake(24, 24, 20, 24);
    page.translatesAutoresizingMaskIntoConstraints = NO;
    NSView *root = self.window.contentView;
    if (root == nil) {
        return;
    }
    [root addSubview:page];
    [NSLayoutConstraint activateConstraints:@[
        [page.topAnchor constraintEqualToAnchor:root.topAnchor],
        [page.leadingAnchor constraintEqualToAnchor:root.leadingAnchor],
        [page.trailingAnchor constraintEqualToAnchor:root.trailingAnchor],
        [buttons.trailingAnchor constraintEqualToAnchor:page.trailingAnchor constant:-24],
    ]];
    _body = page;
    [self updatePolling];
}

/// Once a second, only while the permission or try-it step is showing.
- (void)updatePolling {
    const BOOL want = _step == KFStepPermission || _step == KFStepTry;
    if (want && _poll == nil) {
        __weak KFOnboardingWindowController *weakSelf = self;
        _poll = [NSTimer scheduledTimerWithTimeInterval:1.0
                                                repeats:YES
                                                  block:^(NSTimer *timer) {
                                                      (void)timer;
                                                      [weakSelf poll];
                                                  }];
        _poll.tolerance = 0.2;
        [self poll];
    } else if (!want) {
        [_poll invalidate];
        _poll = nil;
    }
}

- (void)poll {
    const BOOL granted = kf_keytap_has_permission();
    if (granted) {
        [_context.power checkPermission]; /* Starts the tap. */
    }
    const BOOL tapRuns = kf_keytap_is_running();
    _permissionStatus.stringValue = granted && tapRuns ? @"Allowed. You are all set."
                                    : granted ? @"Allowed, but macOS needs Keyfeel to restart."
                                              : @"Not allowed yet.";
    _relaunch.hidden = !(granted && !tapRuns);
    if (_step == KFStepTry) {
        const float v = kf_sensor_last_velocity();
        _tier.stringValue = v >= 0.0f ? [NSString stringWithFormat:@"Last keystroke: %.0f%% force",
                                                                   (double)v * 100.0]
                                      : (tapRuns ? @"Every key plays at the same strength on "
                                                   @"this Mac."
                                                 : @"Allow Input Monitoring first.");
    }
}

#pragma mark - Actions

- (void)requestPermission:(id)sender {
    (void)sender;
    kf_keytap_request_permission();
}

- (void)openSettings:(id)sender {
    (void)sender;
    NSURL *url = [NSURL URLWithString:kInputMonitoringSettings];
    if (url != nil) {
        [NSWorkspace.sharedWorkspace openURL:url];
    }
}

- (void)relaunch:(id)sender {
    (void)sender;
    [_context.actions relaunch];
}

- (void)toggleStats:(NSButton *)sender {
    _context.preferences.telemetryOptIn = sender.state == NSControlStateValueOn;
}

- (void)back:(id)sender {
    (void)sender;
    if (_step > KFStepWelcome) {
        [self show:(KFOnboardingStep)(_step - 1)];
    }
}

- (void)next:(id)sender {
    (void)sender;
    if (_step + 1 < KFStepCount) {
        [self show:(KFOnboardingStep)(_step + 1)];
    } else {
        _context.preferences.onboardingDone = YES;
        [self close];
    }
}

- (void)windowWillClose:(NSNotification *)notification {
    (void)notification;
    [_poll invalidate];
    _poll = nil;
    if (self.onClose != nil) {
        self.onClose();
    }
}

@end
