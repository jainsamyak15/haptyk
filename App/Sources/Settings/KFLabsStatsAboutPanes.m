// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "Settings/KFSettingsPanes.h"

#import <ApplicationServices/ApplicationServices.h>

#include <kf/kf_config.h>

#import "Packs/KFPackLibrary.h"
#import "Power/KFPowerController.h"
#import "Preferences/KFPreferences.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "Stats/KFBadge.h"
#import "Stats/KFStatsController.h"
#import "Support/KFLog.h"
#import "Support/KFUI.h"
#import "Support/KFUpdater.h"
#import "Telemetry/KFDiagnostics.h"

static const CGFloat kPaneWidth = 560.0;

#pragma mark - Labs

@implementation KFLabsPane {
    NSButton *_bigEnter;
    NSButton *_shout;
    NSButton *_meter;
    NSTextField *_shoutStatus;
    NSButton *_openAccessibility;
    NSTextField *_forceNote;
}

- (void)loadView {
    self.title = @"Labs";
    _bigEnter = KFCheckbox(@"Big Enter", self, @selector(toggle:));
    _shout = KFCheckbox(@"Shout Mode", self, @selector(toggle:));
    _meter = KFCheckbox(@"Force Meter", self, @selector(toggle:));
    _shoutStatus = KFSecondaryLabel(@"");
    _openAccessibility =
        KFPushButton(@"Open Accessibility Settings…", self, @selector(openAccessibility:));
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
                                     @"is ever changed, and never in password fields."),
                    _shoutStatus, _openAccessibility
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

- (void)viewDidLoad {
    [super viewDidLoad];
    /* Back from System Settings: Accessibility may have been granted. No polling. */
    [NSNotificationCenter.defaultCenter addObserver:self
                                           selector:@selector(appBecameActive:)
                                               name:NSApplicationDidBecomeActiveNotification
                                             object:nil];
}

- (void)viewWillAppear {
    [super viewWillAppear];
    [self.context.power checkPermission];
}

- (void)appBecameActive:(NSNotification *)note {
    (void)note;
    if (self.viewLoaded && self.view.window.visible) {
        [self.context.power checkPermission];
    }
}

+ (NSString *)textForShoutStatus:(KFShoutStatus)status {
    switch (status) {
    case KFShoutStatusOff:
        return AXIsProcessTrusted() ? @"Accessibility: allowed."
                                    : @"Turning Shout Mode on asks for Accessibility.";
    case KFShoutStatusOn:
        return @"On: hard typing types capitals.";
    case KFShoutStatusNeedsAccessibility:
        return @"Waiting for Accessibility. Allow Keyfeel in System Settings → Privacy & "
               @"Security → Accessibility; until then keys pass through unchanged.";
    case KFShoutStatusUnavailable:
        return @"Force detection is not available on this Mac, so Shout Mode stays idle.";
    case KFShoutStatusPaused:
        return @"Paused while Keyfeel's sounds are off.";
    case KFShoutStatusTripped:
        return @"Turned off: macOS kept pausing the keyboard tap, so Keyfeel stepped aside. "
               @"Turn it on again to retry.";
    }
    return @"";
}

- (void)refresh {
    KFPreferences *prefs = self.context.preferences;
    _bigEnter.state = prefs.labsBigEnter;
    _shout.state = prefs.labsShoutMode;
    _meter.state = prefs.labsForceMeter;
    const KFShoutStatus status = self.context.power.shoutStatus;
    _shoutStatus.stringValue = [KFLabsPane textForShoutStatus:status];
    _openAccessibility.hidden = status != KFShoutStatusNeedsAccessibility;
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
        prefs.labsShoutMode = on; /* The power controller asks for Accessibility if useful. */
    }
}

/// Opens the pane only: the user decides there. The app never touches the TCC database.
- (void)openAccessibility:(id)sender {
    (void)sender;
    NSURL *url = [NSURL
        URLWithString:
            @"x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility"];
    if (url != nil) {
        [NSWorkspace.sharedWorkspace openURL:url];
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
    NSTextField *_badge;
    NSButton *_share;
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
    _badge = KFLabel(@"");
    _share = KFPushButton(@"Share Badge…", self, @selector(shareBadge:));
    self.view = KFPane(
        @[
            KFHeading(@"Today"),
            _today,
            KFHeading(@"Force, last 7 days"),
            _histogram,
            KFHStack(@[ _badge, _share ], 12.0),
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
    _histogram.counts = [stats histogramForLastDays:7];
    _histogram.accessibilityLabel = @"Force histogram for the last 7 days";
    _histogram.accessibilityRole = NSAccessibilityImageRole;
    NSUInteger measured = 0;
    for (NSNumber *n in _histogram.counts) {
        measured += n.unsignedIntegerValue;
    }
    _histogram.accessibilityValue =
        measured == 0
            ? @"No measured keystrokes yet"
            : [NSString stringWithFormat:@"%lu measured keystrokes, softer to harder: %@",
                                         (unsigned long)measured,
                                         [_histogram.counts componentsJoinedByString:@", "]];
    [_histogram setNeedsDisplay:YES];
    _optIn.state = self.context.preferences.telemetryOptIn;
    double percentile = 0.0;
    NSInteger badge = 0;
    const BOOL earned = [self.context currentPercentile:&percentile badge:&badge];
    _badge.stringValue =
        earned ? [NSString stringWithFormat:@"%@: harder than %.0f%% of users",
                                            [KFBadge nameOfBadge:(KFBadgeKind)badge], percentile]
               : (self.context.badgeLine
                      ?: @"Badges need force detection and the "
                         @"shared percentile table.");
    _share.enabled = earned;
}

/// Draws the card locally and opens the share sheet: nothing leaves the Mac unless shared.
- (void)shareBadge:(NSButton *)sender {
    double percentile = 0.0;
    NSInteger badge = 0;
    if (![self.context currentPercentile:&percentile badge:&badge]) {
        return;
    }
    NSString *pack =
        [self.context.library resolvePack:self.context.preferences.packId].name ?: @"Keyfeel";
    NSData *png = [KFBadge cardWithBadge:(KFBadgeKind)badge percentile:percentile packName:pack];
    NSString *path = [KFCachesPath() stringByAppendingPathComponent:@"Keyfeel Badge.png"];
    [NSFileManager.defaultManager createDirectoryAtPath:KFCachesPath()
                            withIntermediateDirectories:YES
                                             attributes:nil
                                                  error:NULL];
    if (png == nil || ![png writeToFile:path atomically:YES]) {
        return;
    }
    NSSharingServicePicker *picker =
        [[NSSharingServicePicker alloc] initWithItems:@[ [NSURL fileURLWithPath:path] ]];
    [picker showRelativeToRect:sender.bounds ofView:sender preferredEdge:NSRectEdgeMinY];
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
    NSButton *export = KFPushButton(@"Export Diagnostics…", self, @selector(exportDiagnostics:));
    NSButton *remove = KFPushButton(@"Remove All Keyfeel Data…", self, @selector(removeAll:));
    NSString *sparkle = self.context.updater.available
                            ? @"Updates by Sparkle (MIT licence, sparkle-project.org)."
                            : @"Updates by Sparkle (MIT licence) in signed release builds.";
    self.view = KFPane(
        @[
            KFHeading(@"Keyfeel"),
            KFLabel(version),
            KFSecondaryLabel(info[@"NSHumanReadableCopyright"] ?: @""),
            privacy,
            KFHeading(@"Acknowledgements"),
            KFSecondaryLabel(sparkle),
            KFHeading(@"Sound attributions"),
            _attributions,
            KFHeading(@"Diagnostics"),
            KFSecondaryLabel(@"A text file with versions, audio and sensor state and counters, for "
                             @"bug reports. It never holds key data."),
            export,
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

- (void)exportDiagnostics:(id)sender {
    (void)sender;
    NSSavePanel *panel = [NSSavePanel savePanel];
    panel.nameFieldStringValue = @"Keyfeel Diagnostics.txt";
    panel.allowedContentTypes = @[ UTTypePlainText ];
    NSString *report = [self.context.diagnostics reportForContext:self.context];
    [panel beginWithCompletionHandler:^(NSModalResponse result) {
        NSURL *url = panel.URL;
        if (result == NSModalResponseOK && url != nil) {
            [report writeToURL:url atomically:YES encoding:NSUTF8StringEncoding error:NULL];
        }
    }];
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
