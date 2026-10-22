// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "Labs/KFForceMeterController.h"

#import <AppKit/AppKit.h>
#include <math.h>
#include <stdatomic.h>

#include <kf/kf.h>

#import "Application/KFAppContext.h"
#import "Power/KFPowerController.h"
#import "Preferences/KFPreferences.h"
#import "Stats/KFStatsController.h"

static const CGFloat kWidth = 188.0;
static const CGFloat kHeight = 58.0;
static const CGFloat kMargin = 12.0;
static const uint64_t kLeewayNs = 10u * NSEC_PER_MSEC; /* Lets macOS coalesce the wakeups. */

/// A track and a fill as wide as the force, in the accent colour.
@interface KFForceBarView : NSView
@property (nonatomic) float velocity; ///< Negative: unknown, no fill.
@end

@implementation KFForceBarView

- (void)drawRect:(NSRect)dirty {
    (void)dirty;
    const NSRect track = NSInsetRect(self.bounds, 0.0, 1.0);
    [NSColor.quaternaryLabelColor setFill];
    [[NSBezierPath bezierPathWithRoundedRect:track xRadius:3.0 yRadius:3.0] fill];
    if (self.velocity >= 0.0f) {
        NSRect fill = track;
        fill.size.width = MAX(6.0, track.size.width * (CGFloat)fminf(self.velocity, 1.0f));
        [NSColor.controlAccentColor setFill];
        [[NSBezierPath bezierPathWithRoundedRect:fill xRadius:3.0 yRadius:3.0] fill];
    }
}

@end

@implementation KFForceMeterController {
    KFAppContext *_context;
    dispatch_source_t _timer;
    NSPanel *_panel;
    NSTextField *_value;
    KFForceBarView *_bar;
    uint64_t _keys; /* Counters at the last tick. */
    uint64_t _resolved;
    uint64_t _defaulted;
    BOOL _waiting;     /* A key whose force may still be one sensor window away. */
    uint64_t _shownAt; /* kf_time ns of the last keystroke shown; 0 while hidden. */
}

- (instancetype)initWithContext:(KFAppContext *)context {
    if ((self = [super init])) {
        _context = context;
        for (NSNotificationName name in
             @[ KFPowerStateDidChangeNotification, KFPreferencesDidChangeNotification ]) {
            [NSNotificationCenter.defaultCenter addObserver:self
                                                   selector:@selector(changed:)
                                                       name:name
                                                     object:nil];
        }
        [self update];
    }
    return self;
}

- (void)dealloc {
    [NSNotificationCenter.defaultCenter removeObserver:self];
    [self stop];
}

- (BOOL)isRunning {
    return _timer != nil;
}

#pragma mark - Text

+ (NSString *)tierNameForVelocity:(float)velocity {
    int lo = 0;
    float w = 0.0f;
    kf_tier_weights(velocity >= 0.0f ? fminf(velocity, 1.0f) : 0.0f, &lo, &w);
    const int nearest = w >= 0.5f ? lo + 1 : lo;
    NSArray<NSString *> *names = @[ @"Soft", @"Medium", @"Hard", @"Slam" ];
    return names[(NSUInteger)MAX(0, MIN(nearest, (int)KF_TIER_COUNT - 1))];
}

+ (NSString *)textForVelocity:(float)velocity {
    if (!(velocity >= 0.0f)) {
        return @"Not measured";
    }
    const float v = fminf(velocity, 1.0f);
    return
        [NSString stringWithFormat:@"%.0f%% · %@", (double)v * 100.0, [self tierNameForVelocity:v]];
}

#pragma mark - Running only while typing

- (void)changed:(NSNotification *)note {
    (void)note;
    [self update];
}

- (void)update {
    const BOOL want =
        _context.preferences.labsForceMeter && _context.power.state == KFPowerStateActive;
    if (want && _timer == nil) {
        [self start];
    } else if (!want && _timer != nil) {
        [self stop];
    }
}

- (void)start {
    kf_sensor_counters counters;
    kf_sensor_get_counters(&counters);
    _resolved = counters.keys_resolved;
    _defaulted = counters.keys_defaulted;
    const uint64_t keys = atomic_load_explicit(&_context.stats.live->keys, memory_order_relaxed);
    _keys = keys > 0u ? keys - 1u : 0u; /* The key that woke the app shows too. */
    _waiting = NO;
    const uint64_t period = (uint64_t)KF_FORCE_METER_POLL_MS * NSEC_PER_MSEC;
    __weak KFForceMeterController *weakSelf = self;
    _timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    dispatch_source_set_timer(_timer, dispatch_time(DISPATCH_TIME_NOW, 0), period, kLeewayNs);
    dispatch_source_set_event_handler(_timer, ^{
        [weakSelf tick];
    });
    dispatch_resume(_timer);
}

/// Idle or turned off: no timer, no panel.
- (void)stop {
    if (_timer != nil) {
        dispatch_source_cancel(_timer);
        _timer = nil;
    }
    [_panel orderOut:nil];
    _panel = nil;
    _value = nil;
    _bar = nil;
    _shownAt = 0u;
}

- (void)tick {
    kf_sensor_counters counters;
    kf_sensor_get_counters(&counters);
    if (counters.keys_resolved < _resolved || counters.keys_defaulted < _defaulted) {
        _resolved = counters.keys_resolved; /* The sensor restarted, e.g. a device change. */
        _defaulted = counters.keys_defaulted;
    }
    const uint64_t keys = atomic_load_explicit(&_context.stats.live->keys, memory_order_relaxed);
    const BOOL newForce = counters.keys_resolved != _resolved;
    const BOOL newDefault = counters.keys_defaulted != _defaulted;
    const BOOL newKey = keys != _keys;
    _resolved = counters.keys_resolved;
    _defaulted = counters.keys_defaulted;
    _keys = keys;
    const uint64_t now = kf_time_now_ns();
    if (newForce) {
        [self show:kf_sensor_last_velocity() at:now];
    } else if (newDefault) {
        [self show:-1.0f at:now];
    } else if (newKey || _waiting) {
        if (!_waiting && kf_sensor_state_now(now) == KF_SENSOR_LIVE) {
            _waiting = YES; /* Measured a few ms after key-down: look again next tick. */
            return;
        }
        [self show:-1.0f at:now];
    }
    _waiting = NO;
    if (_shownAt != 0u && now - _shownAt >= (uint64_t)KF_FORCE_METER_SHOW_MS * NSEC_PER_MSEC) {
        [_panel orderOut:nil];
        _shownAt = 0u;
    }
}

#pragma mark - Panel

- (void)show:(float)velocity at:(uint64_t)now {
    if (_panel == nil) {
        [self buildPanel];
    }
    NSString *text = [KFForceMeterController textForVelocity:velocity];
    _value.stringValue = text;
    _bar.velocity = velocity;
    _bar.accessibilityValue = velocity >= 0.0f ? @(fminf(velocity, 1.0f)) : nil;
    [_bar setNeedsDisplay:YES];
    _panel.contentView.accessibilityValue = text;
    if (!_panel.visible) {
        NSScreen *screen = NSScreen.mainScreen ?: NSScreen.screens.firstObject;
        const NSRect area = screen != nil ? screen.visibleFrame : NSZeroRect;
        [_panel setFrameOrigin:NSMakePoint(NSMaxX(area) - kWidth - kMargin,
                                           NSMaxY(area) - kHeight - kMargin)];
        [_panel orderFrontRegardless]; /* Never activates Keyfeel or takes the keyboard. */
    }
    _shownAt = now;
}

- (void)buildPanel {
    NSPanel *panel = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(0, 0, kWidth, kHeight)
                  styleMask:NSWindowStyleMaskBorderless | NSWindowStyleMaskNonactivatingPanel
                    backing:NSBackingStoreBuffered
                      defer:YES];
    panel.level = NSStatusWindowLevel;
    panel.opaque = NO;
    panel.backgroundColor = NSColor.clearColor;
    panel.hasShadow = YES;
    panel.ignoresMouseEvents = YES;
    panel.hidesOnDeactivate = NO;
    panel.releasedWhenClosed = NO;
    panel.animationBehavior = NSWindowAnimationBehaviorNone;
    panel.collectionBehavior =
        NSWindowCollectionBehaviorCanJoinAllSpaces | NSWindowCollectionBehaviorStationary |
        NSWindowCollectionBehaviorFullScreenAuxiliary | NSWindowCollectionBehaviorIgnoresCycle;

    NSVisualEffectView *background =
        [[NSVisualEffectView alloc] initWithFrame:NSMakeRect(0, 0, kWidth, kHeight)];
    background.material = NSVisualEffectMaterialHUDWindow;
    background.blendingMode = NSVisualEffectBlendingModeBehindWindow;
    background.state = NSVisualEffectStateActive;
    background.wantsLayer = YES;
    background.layer.cornerRadius = 10.0;
    background.layer.masksToBounds = YES;
    background.accessibilityElement = YES;
    background.accessibilityRole = NSAccessibilityGroupRole;
    background.accessibilityLabel = @"Force Meter";

    NSTextField *title = [NSTextField labelWithString:@"Force"];
    title.font = [NSFont systemFontOfSize:NSFont.smallSystemFontSize weight:NSFontWeightMedium];
    title.textColor = NSColor.secondaryLabelColor;
    _value = [NSTextField labelWithString:@""];
    _value.font = [NSFont monospacedDigitSystemFontOfSize:15.0 weight:NSFontWeightSemibold];
    _value.alignment = NSTextAlignmentRight;
    _bar = [KFForceBarView new];
    _bar.accessibilityElement = YES;
    _bar.accessibilityRole = NSAccessibilityLevelIndicatorRole;
    _bar.accessibilityLabel = @"Keystroke force";
    [_bar.heightAnchor constraintEqualToConstant:8.0].active = YES;

    NSStackView *top = [NSStackView stackViewWithViews:@[ title, _value ]];
    top.distribution = NSStackViewDistributionEqualSpacing;
    NSStackView *stack = [NSStackView stackViewWithViews:@[ top, _bar ]];
    stack.orientation = NSUserInterfaceLayoutOrientationVertical;
    stack.alignment = NSLayoutAttributeLeading;
    stack.spacing = 6.0;
    stack.edgeInsets = NSEdgeInsetsMake(10, 12, 10, 12);
    stack.translatesAutoresizingMaskIntoConstraints = NO;
    [background addSubview:stack];
    [NSLayoutConstraint activateConstraints:@[
        [stack.leadingAnchor constraintEqualToAnchor:background.leadingAnchor],
        [stack.trailingAnchor constraintEqualToAnchor:background.trailingAnchor],
        [stack.topAnchor constraintEqualToAnchor:background.topAnchor],
        [stack.bottomAnchor constraintEqualToAnchor:background.bottomAnchor],
        [top.widthAnchor constraintEqualToAnchor:stack.widthAnchor constant:-24.0],
        [_bar.widthAnchor constraintEqualToAnchor:top.widthAnchor],
    ]];
    panel.contentView = background;
    _panel = panel;
}

@end
