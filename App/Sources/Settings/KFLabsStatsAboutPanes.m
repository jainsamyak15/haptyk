// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "Settings/KFSettingsPanes.h"

#import <ApplicationServices/ApplicationServices.h>

#include <kf/kf_config.h>

#import "Packs/KFPackLibrary.h"
#import "Power/KFPowerController.h"
#import "Preferences/KFPreferences.h"
#import "Stats/KFStatsController.h"
#import "Support/KFUI.h"

static const CGFloat kPaneWidth = 560.0;

#pragma mark - Labs

@implementation KFLabsPane {
    NSButton *_bigEnter;
    NSButton *_shout;
    NSButton *_meter;
    NSTextField *_shoutStatus;
    NSTextField *_forceNote;
}

- (void)loadView {
    self.title = @"Labs";
    _bigEnter = KFCheckbox(@"Big Enter", self, @selector(toggle:));
    _shout = KFCheckbox(@"Shout Mode", self, @selector(toggle:));
    _meter = KFCheckbox(@"Force Meter", self, @selector(toggle:));
    _shoutStatus = KFSecondaryLabel(@"");
    _forceNote = KFSecondaryLabel(@"");
    self.view = KFPane(
        @[
            KFLabel(@"Extras that react to how hard you type. All are off by default."),
            _forceNote,
            KFVStack(
                @[
                    _bigEnter, KFSecondaryLabel(@"Slamming Enter adds a deep boom on top of the "
                                                @"switch sound.")
                ],
                3.0),
            KFVStack(
                @[
                    _shout,
                    KFSecondaryLabel(@"Typing letters hard types them as capitals. "
                                     @"Needs Accessibility, to add Shift; nothing else "
                                     @"is ever changed."),
                    _shoutStatus
                ],
                3.0),
            KFVStack(
                @[
                    _meter, KFSecondaryLabel(@"A small floating meter shows each keystroke's "
                                             @"force for a second.")
                ],
                3.0),
        ],
        kPaneWidth);
}

- (void)refresh {
    KFPreferences *prefs = self.context.preferences;
    _bigEnter.state = prefs.labsBigEnter;
    _shout.state = prefs.labsShoutMode;
    _meter.state = prefs.labsForceMeter;
    const BOOL trusted = AXIsProcessTrusted();
    _shoutStatus.stringValue = trusted ? @"Accessibility: allowed."
                                       : @"Accessibility: not allowed yet. Turning Shout Mode on "
                                         @"asks for it.";
    _forceNote.stringValue = self.context.power.forceStatus == KFForceStatusUnavailable
                                 ? @"Force detection is not available on this Mac, so these extras "
                                   @"have nothing to react to."
                                 : @"";
}

- (void)toggle:(NSButton *)sender {
    KFPreferences *prefs = self.context.preferences;
    const BOOL on = sender.state == NSControlStateValueOn;
    if (sender == _bigEnter) {
        prefs.labsBigEnter = on;
    } else if (sender == _meter) {
        prefs.labsForceMeter = on;
    } else if (sender == _shout) {
        if (on && !AXIsProcessTrusted()) {
            /* Asked only when the user turns on a feature that needs it (spec: Onboarding). */
            NSDictionary *options = @{(__bridge NSString *)kAXTrustedCheckOptionPrompt : @YES};
            (void)AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef)options);
        }
        prefs.labsShoutMode = on;
    }
}

@end

#pragma mark - Stats

/// The 7-day force histogram: one series, recessive grid, labels in text colors.
@interface KFHistogramView : NSView
@property (nonatomic, copy) NSArray<NSNumber *> *counts;
@end

@implementation KFHistogramView

- (BOOL)isFlipped {
    return NO;
}

- (void)drawRect:(NSRect)dirty {
    (void)dirty;
    const NSRect bounds = NSInsetRect(self.bounds, 4, 18);
    NSUInteger peak = 0;
    for (NSNumber *n in self.counts) {
        peak = MAX(peak, n.unsignedIntegerValue);
    }
    [NSColor.separatorColor set];
    NSRectFill(NSMakeRect(bounds.origin.x, bounds.origin.y, bounds.size.width, 1.0));
    NSDictionary *label = @{
        NSFontAttributeName : [NSFont systemFontOfSize:NSFont.smallSystemFontSize],
        NSForegroundColorAttributeName : NSColor.secondaryLabelColor
    };
    if (peak == 0) {
        [@"No measured keystrokes yet" drawAtPoint:NSMakePoint(bounds.origin.x, NSMidY(bounds))
                                    withAttributes:label];
        return;
    }
    const NSUInteger count = self.counts.count;
    const CGFloat slot = bounds.size.width / (CGFloat)count;
    [NSColor.systemBlueColor set];
    for (NSUInteger i = 0; i < count; ++i) {
        const CGFloat h =
            bounds.size.height * (CGFloat)self.counts[i].unsignedIntegerValue / (CGFloat)peak;
        const NSRect bar = NSMakeRect(bounds.origin.x + slot * (CGFloat)i + 1.0,
                                      bounds.origin.y + 1.0, slot - 2.0, h);
        [[NSBezierPath bezierPathWithRoundedRect:bar xRadius:2 yRadius:2] fill];
    }
    [@"softer" drawAtPoint:NSMakePoint(bounds.origin.x, 2) withAttributes:label];
    NSString *harder = @"harder";
    const NSSize size = [harder sizeWithAttributes:label];
    [harder drawAtPoint:NSMakePoint(NSMaxX(bounds) - size.width, 2) withAttributes:label];
}

@end

@implementation KFStatsPane {
    NSTextField *_today;
    KFHistogramView *_histogram;
    NSButton *_optIn;
}

- (void)loadView {
    self.title = @"Stats";
    _today = KFLabel(@"");
    _histogram = [KFHistogramView new];
    [_histogram.heightAnchor constraintEqualToConstant:110].active = YES;
    [_histogram.widthAnchor constraintEqualToConstant:500].active = YES;
    _optIn = KFCheckbox(@"Share anonymous daily counts to improve Keyfeel", self,
                        @selector(toggleOptIn:));
    self.view = KFPane(
        @[
            KFHeading(@"Today"),
            _today,
            KFHeading(@"Force, last 7 days"),
            _histogram,
            KFHeading(@"Privacy"),
            _optIn,
            KFSecondaryLabel(@"Only counts are shared: keystrokes per day and how hard they were, "
                             @"never which keys, text, apps, windows or URLs. Off by default."),
            KFPushButton(@"Reset Stats…", self, @selector(resetStats:)),
        ],
        kPaneWidth);
}

- (void)refresh {
    KFStatsController *stats = self.context.stats;
    [stats update];
    KFDayStats *today = stats.today;
    _today.stringValue =
        [NSString stringWithFormat:@"%@ keys · %lu active minutes · peak %lu keys a minute",
                                   [NSNumberFormatter
                                       localizedStringFromNumber:@(today.keys)
                                                     numberStyle:NSNumberFormatterDecimalStyle],
                                   (unsigned long)today.activeMinutes,
                                   (unsigned long)today.peakKeysPerMinute];
    NSMutableArray<NSNumber *> *sum = [NSMutableArray array];
    for (NSUInteger i = 0; i < KF_STATS_FORCE_BUCKETS; ++i) {
        [sum addObject:@0];
    }
    for (KFDayStats *day in [stats recentDays:7]) {
        for (NSUInteger i = 0; i < KF_STATS_FORCE_BUCKETS; ++i) {
            sum[i] = @(sum[i].unsignedIntegerValue + day.forceHistogram[i].unsignedIntegerValue);
        }
    }
    _histogram.counts = sum;
    _histogram.accessibilityLabel = @"Force histogram for the last 7 days";
    _histogram.accessibilityRole = NSAccessibilityImageRole;
    [_histogram setNeedsDisplay:YES];
    _optIn.state = self.context.preferences.telemetryOptIn;
}

- (void)toggleOptIn:(NSButton *)sender {
    self.context.preferences.telemetryOptIn = sender.state == NSControlStateValueOn;
}

- (void)resetStats:(id)sender {
    (void)sender;
    NSAlert *alert = [NSAlert new];
    alert.messageText = @"Reset all stats?";
    alert.informativeText = @"Deletes your daily counts and starts a new anonymous ID.";
    [alert addButtonWithTitle:@"Reset"];
    [alert addButtonWithTitle:@"Cancel"];
    alert.buttons.firstObject.hasDestructiveAction = YES;
    [self present:alert
             then:^(NSModalResponse response) {
                 if (response == NSAlertFirstButtonReturn) {
                     [self.context.stats reset];
                     [self.context.preferences regenerateInstallId];
                     [self refresh];
                 }
             }];
}

@end

#pragma mark - About

@implementation KFAboutPane {
    NSTextField *_attributions;
}

- (void)loadView {
    self.title = @"About";
    NSDictionary *info = NSBundle.mainBundle.infoDictionary;
    NSString *version =
        [NSString stringWithFormat:@"Version %@ (%@)", info[@"CFBundleShortVersionString"] ?: @"?",
                                   info[@"CFBundleVersion"] ?: @"?"];
    _attributions = KFSecondaryLabel(@"");
    NSButton *privacy = KFPushButton(@"Privacy Policy", self, @selector(openPrivacy:));
    NSButton *remove = KFPushButton(@"Remove All Keyfeel Data…", self, @selector(removeAll:));
    self.view = KFPane(
        @[
            KFHeading(@"Keyfeel"),
            KFLabel(version),
            KFSecondaryLabel(info[@"NSHumanReadableCopyright"] ?: @""),
            privacy,
            KFHeading(@"Sound attributions"),
            _attributions,
            KFHeading(@"Your data"),
            KFSecondaryLabel(
                @"Removes settings, stats, imported packs and the login item, then quits. "
                @"After that, delete the app to remove Keyfeel completely."),
            remove,
        ],
        kPaneWidth);
}

- (void)refresh {
    NSArray<NSString *> *lines = self.context.library.attributions;
    _attributions.stringValue = lines.count > 0 ? [lines componentsJoinedByString:@"\n"]
                                                : @"No pack requires an attribution.";
}

- (void)openPrivacy:(id)sender {
    (void)sender;
    NSString *text = NSBundle.mainBundle.infoDictionary[@"KFPrivacyPolicyURL"];
    NSURL *url = text.length > 0 ? [NSURL URLWithString:text] : nil;
    if (url != nil) {
        [NSWorkspace.sharedWorkspace openURL:url];
    }
}

- (void)removeAll:(id)sender {
    (void)sender;
    NSAlert *alert = [NSAlert new];
    alert.messageText = @"Remove all Keyfeel data?";
    alert.informativeText = @"Settings, stats, calibration, imported packs and the login item are "
                            @"deleted, and Keyfeel quits. This cannot be undone.";
    [alert addButtonWithTitle:@"Remove and Quit"];
    [alert addButtonWithTitle:@"Cancel"];
    alert.buttons.firstObject.hasDestructiveAction = YES;
    [self present:alert
             then:^(NSModalResponse response) {
                 if (response == NSAlertFirstButtonReturn) {
                     [self.context.actions removeAllDataAndQuit];
                 }
             }];
}

@end
