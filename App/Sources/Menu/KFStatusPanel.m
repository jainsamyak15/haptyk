// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "Menu/KFStatusPanel.h"

#include <kf/kf.h>

#import "Menu/KFFeelView.h"
#import "Menu/KFPackListView.h"
#import "Menu/KFPanelControls.h"
#import "Packs/KFPackLibrary.h"
#import "Power/KFPowerController.h"
#import "Preferences/KFPreferences.h"
#import "Stats/KFBadge.h"
#import "Stats/KFStatsController.h"
#import "Support/KFTheme.h"

static const CGFloat kWidth = 344.0;
static const CGFloat kPad = 16.0;
static const CGFloat kCorner = 16.0;
static const NSTimeInterval kRefresh = 0.5; // Spec: live stats every 0.5 s while open.

/// A borderless panel that can take the keyboard without activating Keyfeel.
@interface KFPanelWindow : NSPanel
@end

@implementation KFPanelWindow

- (BOOL)canBecomeKeyWindow {
    return YES;
}

- (BOOL)canBecomeMainWindow {
    return NO;
}

@end

/// The panel's rounded background and hairline.
@interface KFPanelBackground : NSView
@end

@implementation KFPanelBackground

- (void)drawRect:(NSRect)dirty {
    (void)dirty;
    const NSRect r = NSInsetRect(self.bounds, 0.5, 0.5);
    NSBezierPath *shape = [NSBezierPath bezierPathWithRoundedRect:r
                                                          xRadius:kCorner
                                                          yRadius:kCorner];
    [KFTheme.background setFill];
    [shape fill];
    [KFTheme.stroke setStroke];
    shape.lineWidth = 1.0;
    [shape stroke];
}

@end

/// A one-line notice above the Feel card: missing permission, mic mute, Shout Mode stopped.
@interface KFBanner : KFCardView
@property (nonatomic, readonly) NSTextField *text;
@property (nonatomic, readonly) KFLinkButton *link;
@end

@implementation KFBanner

- (instancetype)initWithFrame:(NSRect)frame {
    if ((self = [super initWithFrame:frame])) {
        self.cornerRadius = 10.0; // Neutral surface: the accent marks the icon and the action.
        NSImage *icon = [NSImage imageWithSystemSymbolName:@"exclamationmark.circle.fill"
                                  accessibilityDescription:@"Needs attention"]
                            ?: [[NSImage alloc] initWithSize:NSMakeSize(14, 14)];
        NSImageView *mark = [NSImageView imageViewWithImage:icon];
        mark.contentTintColor = KFTheme.accent;
        _text = KFThemedLabel(@"", [NSFont systemFontOfSize:12.0 weight:NSFontWeightMedium],
                              KFTheme.textPrimary);
        _link = [KFLinkButton linkWithTitle:@"" target:nil action:nil];
        _link.prominent = YES;
        NSStackView *row = [NSStackView stackViewWithViews:@[ mark, _text, _link ]];
        row.spacing = 8.0;
        row.edgeInsets = NSEdgeInsetsMake(8, 10, 8, 10);
        [_text setContentCompressionResistancePriority:NSLayoutPriorityDefaultLow
                                        forOrientation:NSLayoutConstraintOrientationHorizontal];
        row.translatesAutoresizingMaskIntoConstraints = NO;
        [self addSubview:row];
        [NSLayoutConstraint activateConstraints:@[
            [row.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
            [row.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
            [row.topAnchor constraintEqualToAnchor:self.topAnchor],
            [row.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
        ]];
    }
    return self;
}

@end

/// A one-point rule in the stroke colour.
@interface KFHairline : NSView
@end

@implementation KFHairline

- (void)drawRect:(NSRect)dirty {
    (void)dirty;
    [KFTheme.stroke setFill];
    NSRectFill(self.bounds);
}

@end

@interface KFStatusPanelController () <NSWindowDelegate>
@end

@implementation KFStatusPanelController {
    KFAppContext *_context;
    KFPanelWindow *_panel;
    NSStackView *_stack;
    KFPillSwitch *_power;
    NSTextField *_subtitle;
    KFBanner *_banner;
    SEL _bannerAction;
    KFFeelView *_feel;
    KFLevelSlider *_volume;
    KFPackListView *_packs;
    KFStatTile *_keysTile;
    KFStatTile *_tapsTile;
    KFStatTile *_thirdTile;
    KFChipButton *_bigEnter;
    KFChipButton *_shout;
    KFChipButton *_meter;
    NSTimer *_timer;
    id _outsideMonitor;
    id _keyMonitor;
}

- (instancetype)initWithContext:(KFAppContext *)context {
    if ((self = [super init])) {
        _context = context;
        for (NSNotificationName name in @[
                 KFPowerStateDidChangeNotification, KFPreferencesDidChangeNotification,
                 KFPackLibraryDidChangeNotification
             ]) {
            [NSNotificationCenter.defaultCenter addObserver:self
                                                   selector:@selector(changed:)
                                                       name:name
                                                     object:nil];
        }
    }
    return self;
}

- (void)dealloc {
    [NSNotificationCenter.defaultCenter removeObserver:self];
    [self close];
}

- (BOOL)isShown {
    return _panel.visible;
}

#pragma mark - Building

static NSView *Hairline(void) {
    KFHairline *line = [KFHairline new];
    [line.heightAnchor constraintEqualToConstant:1.0].active = YES;
    return line;
}

static NSImageView *Icon(NSString *symbol, NSString *description) {
    NSImage *image = [NSImage imageWithSystemSymbolName:symbol accessibilityDescription:description]
                         ?: [[NSImage alloc] initWithSize:NSMakeSize(14, 14)];
    NSImageView *view = [NSImageView imageViewWithImage:image];
    view.contentTintColor = KFTheme.textTertiary;
    view.symbolConfiguration =
        [NSImageSymbolConfiguration configurationWithPointSize:12.0 weight:NSFontWeightMedium];
    view.accessibilityElement = NO;
    return view;
}

- (NSView *)header {
    NSTextField *title = KFThemedLabel(
        @"Keyfeel", [NSFont systemFontOfSize:16.0 weight:NSFontWeightBold], KFTheme.textPrimary);
    NSTextField *dot =
        KFThemedLabel(@".", [NSFont systemFontOfSize:16.0 weight:NSFontWeightBold], KFTheme.accent);
    dot.accessibilityElement = NO;
    NSStackView *word = [NSStackView stackViewWithViews:@[ title, dot ]];
    word.spacing = 0.0;
    _subtitle = KFThemedLabel(@"", [NSFont systemFontOfSize:11.5 weight:NSFontWeightRegular],
                              KFTheme.textTertiary);
    NSStackView *left = [NSStackView stackViewWithViews:@[ word, _subtitle ]];
    left.orientation = NSUserInterfaceLayoutOrientationVertical;
    left.alignment = NSLayoutAttributeLeading;
    left.spacing = 1.0;
    _power = [KFPillSwitch new];
    _power.target = self;
    _power.action = @selector(togglePower:);
    _power.accessibilityLabel = @"Sounds";
    _power.toolTip = @"Turn sounds on or off";
    NSStackView *row = [NSStackView stackViewWithViews:@[ left, _power ]];
    row.distribution = NSStackViewDistributionEqualSpacing;
    return row;
}

- (NSView *)volumeRow {
    _volume = [KFLevelSlider new];
    _volume.target = self;
    _volume.action = @selector(volumeChanged:);
    _volume.accessibilityLabel = @"Volume";
    NSStackView *row = [NSStackView stackViewWithViews:@[
        Icon(@"speaker.fill", @"Quieter"), _volume, Icon(@"speaker.wave.3.fill", @"Louder")
    ]];
    row.spacing = 10.0;
    return row;
}

- (NSView *)packSection {
    _packs = [KFPackListView new];
    __weak KFStatusPanelController *weakSelf = self;
    _packs.onChoose = ^(KFPackManifest *pack) {
        [weakSelf choosePack:pack];
    };
    NSTextField *label = KFSectionLabel(@"Sound pack");
    KFLinkButton *more = [KFLinkButton linkWithTitle:@"More packs…"
                                              target:self
                                              action:@selector(showPacks:)];
    more.font = [NSFont systemFontOfSize:11.0 weight:NSFontWeightMedium];
    NSStackView *top = [NSStackView stackViewWithViews:@[ label, more ]];
    top.distribution = NSStackViewDistributionEqualSpacing;
    NSStackView *section = [NSStackView stackViewWithViews:@[ top, _packs ]];
    section.orientation = NSUserInterfaceLayoutOrientationVertical;
    section.alignment = NSLayoutAttributeLeading;
    section.spacing = 8.0;
    [top.widthAnchor constraintEqualToAnchor:section.widthAnchor].active = YES;
    [_packs.widthAnchor constraintEqualToAnchor:section.widthAnchor].active = YES;
    return section;
}

- (NSView *)tiles {
    _keysTile = [[KFStatTile alloc] initWithLabel:@"Keys today"];
    _tapsTile = [[KFStatTile alloc] initWithLabel:@"Taps / min"];
    _thirdTile = [[KFStatTile alloc] initWithLabel:@"Active today"];
    NSStackView *row = [NSStackView stackViewWithViews:@[ _keysTile, _tapsTile, _thirdTile ]];
    row.distribution = NSStackViewDistributionFillEqually;
    row.spacing = 8.0;
    return row;
}

- (NSView *)labsRow {
    _bigEnter = [KFChipButton chipWithTitle:@"Big Enter" target:self action:@selector(toggleLab:)];
    _shout = [KFChipButton chipWithTitle:@"Shout Mode" target:self action:@selector(toggleLab:)];
    _meter = [KFChipButton chipWithTitle:@"Force Meter" target:self action:@selector(toggleLab:)];
    _bigEnter.toolTip = @"Slamming Enter adds a deep boom.";
    _shout.toolTip = @"Typing letters hard types them as capitals. Needs Accessibility.";
    _meter.toolTip = @"A small floating meter shows each keystroke's force.";
    KFLinkButton *about = [KFLinkButton linkWithTitle:@"About"
                                               target:self
                                               action:@selector(showLabs:)];
    about.font = [NSFont systemFontOfSize:11.0 weight:NSFontWeightMedium];
    about.accessibilityLabel = @"About Labs";
    NSTextField *label = KFSectionLabel(@"Labs");
    NSStackView *chips = [NSStackView stackViewWithViews:@[ _bigEnter, _shout, _meter ]];
    chips.spacing = 6.0;
    NSStackView *top = [NSStackView stackViewWithViews:@[ label, about ]];
    top.distribution = NSStackViewDistributionEqualSpacing;
    NSStackView *section = [NSStackView stackViewWithViews:@[ top, chips ]];
    section.orientation = NSUserInterfaceLayoutOrientationVertical;
    section.alignment = NSLayoutAttributeLeading;
    section.spacing = 8.0;
    [top.widthAnchor constraintEqualToAnchor:section.widthAnchor].active = YES;
    return section;
}

- (NSView *)footer {
    KFLinkButton *calibrate = [KFLinkButton linkWithTitle:@"Calibrate…"
                                                   target:self
                                                   action:@selector(showCalibration:)];
    KFLinkButton *updates = [KFLinkButton linkWithTitle:@"Check for Updates…"
                                                 target:self
                                                 action:@selector(checkForUpdates:)];
    KFLinkButton *feedback = [KFLinkButton linkWithTitle:@"Send Feedback…"
                                                  target:self
                                                  action:@selector(sendFeedback:)];
    KFLinkButton *settings = [KFLinkButton linkWithTitle:@"Settings…"
                                                  target:self
                                                  action:@selector(showSettings:)];
    settings.keyEquivalent = @",";
    settings.keyEquivalentModifierMask = NSEventModifierFlagCommand;
    KFLinkButton *quit = [KFLinkButton linkWithTitle:@"Quit"
                                              target:NSApp
                                              action:@selector(terminate:)];
    quit.keyEquivalent = @"q";
    quit.keyEquivalentModifierMask = NSEventModifierFlagCommand;
    quit.toolTip = @"Quit Keyfeel (⌘Q)";
    quit.accessibilityLabel = @"Quit Keyfeel";
    NSStackView *first = [NSStackView stackViewWithViews:@[ settings, calibrate, feedback ]];
    first.spacing = 14.0;
    NSStackView *second = [NSStackView stackViewWithViews:@[ updates, quit ]];
    second.distribution = NSStackViewDistributionEqualSpacing;
    NSStackView *footer = [NSStackView stackViewWithViews:@[ first, second ]];
    footer.orientation = NSUserInterfaceLayoutOrientationVertical;
    footer.alignment = NSLayoutAttributeLeading;
    footer.spacing = 4.0;
    [second.widthAnchor constraintEqualToAnchor:footer.widthAnchor].active = YES;
    return footer;
}

- (void)build {
    _banner = [KFBanner new];
    _banner.link.target = self;
    _banner.link.action = @selector(bannerAction:);
    _feel = [KFFeelView new];
    NSArray<NSView *> *rows = @[
        [self header], _banner, _feel, [self volumeRow], [self packSection], [self tiles],
        [self labsRow], Hairline(), [self footer]
    ];
    _stack = [NSStackView stackViewWithViews:rows];
    _stack.orientation = NSUserInterfaceLayoutOrientationVertical;
    _stack.alignment = NSLayoutAttributeLeading;
    _stack.spacing = 14.0;
    [_stack setCustomSpacing:12.0 afterView:rows[0]];
    [_stack setCustomSpacing:10.0 afterView:rows[7]];
    _stack.edgeInsets = NSEdgeInsetsMake(kPad, kPad, kPad - 4.0, kPad);
    _stack.translatesAutoresizingMaskIntoConstraints = NO;
    for (NSView *row in rows) {
        [row.widthAnchor constraintEqualToConstant:kWidth - 2.0 * kPad].active = YES;
    }
    KFPanelBackground *background =
        [[KFPanelBackground alloc] initWithFrame:NSMakeRect(0, 0, kWidth, 600)];
    [background addSubview:_stack];
    [NSLayoutConstraint activateConstraints:@[
        [_stack.topAnchor constraintEqualToAnchor:background.topAnchor],
        [_stack.leadingAnchor constraintEqualToAnchor:background.leadingAnchor],
        [_stack.trailingAnchor constraintEqualToAnchor:background.trailingAnchor],
        [_stack.bottomAnchor constraintEqualToAnchor:background.bottomAnchor],
        [background.widthAnchor constraintEqualToConstant:kWidth],
    ]];
    background.accessibilityElement = YES;
    background.accessibilityRole = NSAccessibilityGroupRole;
    background.accessibilityLabel = @"Keyfeel";

    _panel = [[KFPanelWindow alloc]
        initWithContentRect:NSMakeRect(0, 0, kWidth, 600)
                  styleMask:NSWindowStyleMaskBorderless | NSWindowStyleMaskNonactivatingPanel
                    backing:NSBackingStoreBuffered
                      defer:YES];
    _panel.contentView = background;
    _panel.opaque = NO;
    _panel.backgroundColor = NSColor.clearColor;
    _panel.hasShadow = YES;
    _panel.level = NSPopUpMenuWindowLevel;
    _panel.hidesOnDeactivate = NO;
    _panel.releasedWhenClosed = NO;
    _panel.movable = NO;
    _panel.animationBehavior = KFTheme.reduceMotion ? NSWindowAnimationBehaviorNone
                                                    : NSWindowAnimationBehaviorUtilityWindow;
    _panel.collectionBehavior = NSWindowCollectionBehaviorMoveToActiveSpace |
                                NSWindowCollectionBehaviorFullScreenAuxiliary |
                                NSWindowCollectionBehaviorIgnoresCycle;
    _panel.delegate = self;
    _panel.accessibilityLabel = @"Keyfeel";
}

#pragma mark - Showing

- (void)toggleBelowButton:(NSStatusBarButton *)button {
    if (self.shown) {
        [self close];
        return;
    }
    if (_panel == nil) {
        [self build];
    }
    [_context.power checkPermission];
    [self refreshAll];
    [_panel.contentView layoutSubtreeIfNeeded];
    const NSSize size = _panel.contentView.fittingSize;
    NSRect anchor = [button convertRect:button.bounds toView:nil];
    anchor = button.window != nil ? [button.window convertRectToScreen:anchor] : NSZeroRect;
    NSScreen *screen = button.window.screen ?: NSScreen.mainScreen;
    const NSRect area = screen != nil ? screen.visibleFrame : NSMakeRect(0, 0, 1440, 900);
    CGFloat x = NSMidX(anchor) - size.width / 2.0;
    x = MAX(NSMinX(area) + 8.0, MIN(x, NSMaxX(area) - size.width - 8.0));
    const CGFloat top = NSMinY(anchor) > 0 ? NSMinY(anchor) - 6.0 : NSMaxY(area) - 6.0;
    [_panel setFrame:NSMakeRect(x, top - size.height, size.width, size.height) display:NO];
    [_packs revealCurrent];
    [_panel makeKeyAndOrderFront:nil];
    [_panel invalidateShadow];
    [self startWatching];
}

- (void)close {
    [self stopWatching];
    [_panel orderOut:nil];
}

- (void)startWatching {
    [self stopWatching];
    __weak KFStatusPanelController *weakSelf = self;
    _timer = [NSTimer timerWithTimeInterval:kRefresh
                                    repeats:YES
                                      block:^(NSTimer *timer) {
                                          (void)timer;
                                          [weakSelf refreshLive];
                                      }];
    _timer.tolerance = 0.1;
    [NSRunLoop.mainRunLoop addTimer:_timer forMode:NSRunLoopCommonModes];
    // A click in another app, or on the desktop, closes the panel, as a menu would.
    _outsideMonitor = [NSEvent
        addGlobalMonitorForEventsMatchingMask:NSEventMaskLeftMouseDown | NSEventMaskRightMouseDown
                                      handler:^(NSEvent *event) {
                                          (void)event;
                                          [weakSelf close];
                                      }];
    _keyMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown
                                                        handler:^NSEvent *(NSEvent *event) {
                                                            if (event.keyCode == 53) { // Escape
                                                                [weakSelf close];
                                                                return nil;
                                                            }
                                                            return event;
                                                        }];
}

- (void)stopWatching {
    [_timer invalidate];
    _timer = nil;
    if (_outsideMonitor != nil) {
        [NSEvent removeMonitor:_outsideMonitor];
        _outsideMonitor = nil;
    }
    if (_keyMonitor != nil) {
        [NSEvent removeMonitor:_keyMonitor];
        _keyMonitor = nil;
    }
}

- (void)windowDidResignKey:(NSNotification *)notification {
    (void)notification;
    [self close];
}

#pragma mark - Refreshing

- (void)changed:(NSNotification *)note {
    (void)note;
    if (self.shown) {
        [self refreshAll];
    }
}

- (NSString *)forceText {
    switch (_context.power.forceStatus) {
    case KFForceStatusLive:
        return @"Force: Live";
    case KFForceStatusExternalKeyboard:
        return @"Force: Off (external keyboard)";
    case KFForceStatusUnavailable:
        return @"Force: Not on this Mac";
    case KFForceStatusNeedsCalibration:
        return @"Force: Calibrate";
    }
    return @"";
}

- (void)refreshBanner {
    NSString *text = nil;
    NSString *link = nil;
    SEL action = NULL;
    if (_context.power.state == KFPowerStateNoPermission) {
        text = @"Keyfeel needs Input Monitoring.";
        link = @"Grant…";
        action = @selector(showOnboarding:);
    } else if (_context.power.shoutStatus == KFShoutStatusTripped) {
        text = @"Shout Mode turned itself off.";
        link = @"Labs…";
        action = @selector(showLabs:);
    } else if (_context.power.microphoneMuted) {
        text = @"Muted while the microphone is in use.";
    }
    _banner.hidden = text == nil;
    _banner.text.stringValue = text ?: @"";
    _banner.link.title = link ?: @"";
    _banner.link.hidden = link == nil;
    [_banner.link invalidateIntrinsicContentSize];
    _bannerAction = action;
}

- (void)refreshAll {
    KFPreferences *prefs = _context.preferences;
    _power.on = prefs.enabled;
    KFPackManifest *current = [_context.library resolvePack:prefs.packId];
    _subtitle.stringValue =
        !prefs.enabled ? @"Sounds off" : (current != nil ? current.name : @"No sound pack");
    _volume.doubleValue = prefs.volume;
    _packs.packs = _context.library.packs;
    _packs.currentIdentifier = current.identifier;
    _bigEnter.on = prefs.labsBigEnter;
    _shout.on = prefs.labsShoutMode;
    _meter.on = prefs.labsForceMeter;
    [self refreshBanner];
    [self refreshLive];
    [_panel.contentView layoutSubtreeIfNeeded];
    if (self.shown) { // The banner may have appeared or gone: keep the top edge in place.
        const NSSize size = _panel.contentView.fittingSize;
        NSRect frame = _panel.frame;
        frame.origin.y = NSMaxY(frame) - size.height;
        frame.size.height = size.height;
        [_panel setFrame:frame display:YES];
        [_panel invalidateShadow];
    }
}

- (void)refreshLive {
    const BOOL measured = _context.power.forceStatus == KFForceStatusLive ||
                          _context.power.forceStatus == KFForceStatusNeedsCalibration;
    [_feel setVelocity:measured ? kf_sensor_last_velocity() : -1.0f status:[self forceText]];
    uint32_t seconds[60];
    kf_live_stats_seconds(_context.stats.live, kf_time_now_ns(), seconds,
                          (uint32_t)KFFeelActivitySeconds);
    [_context.stats update]; // In memory: the file is written at most every 5 minutes.
    [_feel setActivity:seconds tapsPerMinute:_context.stats.tapsPerMinute];
    _keysTile.value = [NSNumberFormatter localizedStringFromNumber:@(_context.stats.today.keys)
                                                       numberStyle:NSNumberFormatterDecimalStyle];
    _tapsTile.value =
        [NSString stringWithFormat:@"%lu", (unsigned long)_context.stats.tapsPerMinute];
    double percentile = 0.0;
    NSInteger badge = 0;
    if ([_context currentPercentile:&percentile badge:&badge]) {
        _thirdTile.label = [KFBadge nameOfBadge:(KFBadgeKind)badge];
        _thirdTile.value = [NSString stringWithFormat:@"%.0f%%", percentile];
        _thirdTile.toolTip = [NSString stringWithFormat:@"Harder than %.0f%% of users", percentile];
    } else {
        _thirdTile.label = @"Active today";
        _thirdTile.value = [NSString
            stringWithFormat:@"%lu min", (unsigned long)_context.stats.today.activeMinutes];
        _thirdTile.toolTip = nil;
    }
}

#pragma mark - Actions

- (void)togglePower:(KFPillSwitch *)sender {
    _context.preferences.enabled = sender.on;
}

- (void)volumeChanged:(NSSlider *)sender {
    _context.preferences.volume = (float)sender.doubleValue;
}

- (void)choosePack:(KFPackManifest *)pack {
    _context.preferences.packId = pack.identifier;
    if (_context.preferences.enabled) {
        // A taste of the new pack once it is decoded (a background load of tens of ms).
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(120 * NSEC_PER_MSEC)),
                       dispatch_get_main_queue(), ^{
                           [self->_context.power playTestKeyWithVelocity:0.5f group:KF_GROUP_ALPHA];
                       });
    }
}

- (void)toggleLab:(KFChipButton *)sender {
    KFPreferences *prefs = _context.preferences;
    if (sender == _bigEnter) {
        prefs.labsBigEnter = sender.on;
    } else if (sender == _shout) {
        prefs.labsShoutMode = sender.on;
    } else if (sender == _meter) {
        prefs.labsForceMeter = sender.on;
    }
}

- (void)bannerAction:(id)sender {
    if (_bannerAction != NULL) {
        [NSApp sendAction:_bannerAction to:self from:sender];
    }
}

- (void)closeThen:(void (^)(void))action {
    [self close];
    action();
}

- (void)showOnboarding:(id)sender {
    (void)sender;
    [self closeThen:^{
        [self->_context.actions showOnboarding];
    }];
}

- (void)showPacks:(id)sender {
    (void)sender;
    [self closeThen:^{
        [self->_context.actions showSettingsTab:@"packs"];
    }];
}

- (void)showLabs:(id)sender {
    (void)sender;
    [self closeThen:^{
        [self->_context.actions showSettingsTab:@"labs"];
    }];
}

- (void)showSettings:(id)sender {
    (void)sender;
    [self closeThen:^{
        [self->_context.actions showSettingsTab:@"general"];
    }];
}

- (void)showCalibration:(id)sender {
    (void)sender;
    [self closeThen:^{
        [self->_context.actions showCalibration];
    }];
}

- (void)checkForUpdates:(id)sender {
    (void)sender;
    [self closeThen:^{
        [self->_context.actions checkForUpdates];
    }];
}

- (void)sendFeedback:(id)sender {
    (void)sender;
    [self closeThen:^{
        [self->_context.actions sendFeedback];
    }];
}

#pragma mark - Preview

/// Example values for the "in use" preview: a measured hard keystroke, a minute of typing.
- (void)fillDemo {
    [_feel setVelocity:0.72f status:@"Force: Live"];
    uint32_t seconds[60];
    for (NSUInteger i = 0; i < 60; ++i) {
        const double wave = 4.0 + 3.5 * sin((double)i / 5.0) + 2.0 * sin((double)i / 1.7);
        seconds[i] = i % 17 == 9 ? 0u : (uint32_t)MAX(0.0, wave);
    }
    [_feel setActivity:seconds tapsPerMinute:312];
    _keysTile.value = @"4,812";
    _tapsTile.value = @"312";
    _thirdTile.label = @"Hammer";
    _thirdTile.value = @"81%";
    _banner.hidden = YES;
    _bigEnter.on = YES;
}

- (BOOL)renderPreviewToDirectory:(NSURL *)directory {
    if (_panel == nil) {
        [self build];
    }
    BOOL ok = [self renderTo:directory suffix:@""];
    [self fillDemo];
    ok = [self renderTo:directory suffix:@"-in-use"] && ok;
    [self refreshAll];
    return ok;
}

- (BOOL)renderTo:(NSURL *)directory suffix:(NSString *)suffix {
    if (suffix.length == 0) {
        [self refreshAll];
    }
    [NSFileManager.defaultManager createDirectoryAtURL:directory
                           withIntermediateDirectories:YES
                                            attributes:nil
                                                 error:NULL];
    BOOL ok = YES;
    for (NSString *mode in @[ @"light", @"dark" ]) {
        NSView *view = _panel.contentView;
        view.appearance =
            [NSAppearance appearanceNamed:[mode isEqualToString:@"dark"] ? NSAppearanceNameDarkAqua
                                                                         : NSAppearanceNameAqua];
        [view layoutSubtreeIfNeeded];
        const NSSize size = view.fittingSize;
        [_panel setFrame:NSMakeRect(-10000, -10000, size.width, size.height) display:YES];
        [view layoutSubtreeIfNeeded];
        [view displayIfNeeded];
        NSBitmapImageRep *rep = [view bitmapImageRepForCachingDisplayInRect:view.bounds];
        rep.size = view.bounds.size;
        [view cacheDisplayInRect:view.bounds toBitmapImageRep:rep];
        NSData *png = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
        NSURL *file =
            [directory URLByAppendingPathComponent:[NSString stringWithFormat:@"panel-%@%@.png",
                                                                              mode, suffix]];
        ok = ok && png != nil && file != nil &&
             [png writeToURL:(NSURL *_Nonnull)file atomically:YES];
    }
    _panel.contentView.appearance = nil;
    return ok;
}

@end
