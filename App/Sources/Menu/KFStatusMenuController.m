// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "Menu/KFStatusMenuController.h"

#include <kf/kf.h>

#import "Menu/KFStatusIcon.h"
#import "Menu/KFStatusPanel.h"
#import "Menu/KFVolumeMenuView.h"
#import "Packs/KFPackLibrary.h"
#import "Power/KFPowerController.h"
#import "Preferences/KFPreferences.h"
#import "Stats/KFStatsController.h"
#import "Telemetry/KFTelemetry.h"

static const NSTimeInterval kLiveRefresh = 0.5;

/// Menu header: the app name and the on/off switch.
@interface KFMenuHeaderView : NSView
@property (nonatomic, readonly) NSSwitch *toggle;
@end

@implementation KFMenuHeaderView

- (instancetype)initWithTarget:(id)target action:(SEL)action {
    if ((self = [super initWithFrame:NSMakeRect(0, 0, 260, 34)])) {
        NSTextField *title = [NSTextField labelWithString:@"Keyfeel"];
        title.font = [NSFont boldSystemFontOfSize:NSFont.systemFontSize];
        _toggle = [NSSwitch new];
        _toggle.target = target;
        _toggle.action = action;
        _toggle.accessibilityLabel = @"Sounds on";
        NSStackView *row = [NSStackView stackViewWithViews:@[ title, _toggle ]];
        row.distribution = NSStackViewDistributionEqualSpacing;
        row.edgeInsets = NSEdgeInsetsMake(4, 20, 4, 14);
        row.translatesAutoresizingMaskIntoConstraints = NO;
        [self addSubview:row];
        [NSLayoutConstraint activateConstraints:@[
            [row.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
            [row.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
            [row.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
        ]];
    }
    return self;
}

@end

@implementation KFStatusMenuController {
    KFAppContext *_context;
    NSStatusItem *_item;
    NSMenu *_menu;
    KFMenuHeaderView *_header;
    KFVolumeMenuView *_volume;
    NSMenuItem *_permissionItem;
    NSMenuItem *_shoutTripItem;
    NSMenuItem *_micItem;
    NSMenuItem *_packsEnd; /* Pack items sit above this "More packs…" item. */
    NSMutableArray<NSMenuItem *> *_packItems;
    NSMenuItem *_forceItem;
    NSMenuItem *_lastForceItem;
    NSMenuItem *_keysItem;
    NSMenuItem *_tapsItem;
    NSMenuItem *_badgeItem;
    NSMenuItem *_bigEnterItem;
    NSMenuItem *_shoutItem;
    NSMenuItem *_meterItem;
    NSMenuItem *_shoutStatusItem;
    NSTimer *_liveTimer;
    KFStatusPanelController *_panel;
}

- (instancetype)initWithContext:(KFAppContext *)context {
    if ((self = [super init])) {
        _context = context;
        _packItems = [NSMutableArray array];
        _item = [NSStatusBar.systemStatusBar statusItemWithLength:NSSquareStatusItemLength];
        _item.button.toolTip = @"Keyfeel";
        [self buildMenu];
        _panel = [[KFStatusPanelController alloc] initWithContext:context];
        // Left click: the panel. Right click or Control-click: the classic menu, with every
        // command (spec: Menu), for people who prefer a menu.
        _item.button.target = self;
        _item.button.action = @selector(statusItemClicked:);
        [_item.button sendActionOn:NSEventMaskLeftMouseUp | NSEventMaskRightMouseUp];
        [NSNotificationCenter.defaultCenter addObserver:self
                                               selector:@selector(stateChanged:)
                                                   name:KFPowerStateDidChangeNotification
                                                 object:nil];
        [NSNotificationCenter.defaultCenter addObserver:self
                                               selector:@selector(packsChanged:)
                                                   name:KFPackLibraryDidChangeNotification
                                                 object:nil];
        [NSNotificationCenter.defaultCenter addObserver:self
                                               selector:@selector(stateChanged:)
                                                   name:KFPreferencesDidChangeNotification
                                                 object:nil];
        [self rebuildPacks];
        [self refreshIcon];
    }
    return self;
}

- (void)dealloc {
    [NSNotificationCenter.defaultCenter removeObserver:self];
    [NSStatusBar.systemStatusBar removeStatusItem:_item];
}

#pragma mark - Building

static NSMenuItem *Info(NSString *title) {
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:nil keyEquivalent:@""];
    item.enabled = NO;
    return item;
}

- (NSMenuItem *)action:(NSString *)title selector:(SEL)selector key:(NSString *)key {
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:selector keyEquivalent:key];
    item.target = self;
    return item;
}

- (void)buildMenu {
    _menu = [[NSMenu alloc] initWithTitle:@"Keyfeel"];
    _menu.delegate = self;
    _menu.autoenablesItems = NO;

    NSMenuItem *header = [NSMenuItem new];
    _header = [[KFMenuHeaderView alloc] initWithTarget:self action:@selector(toggleEnabled:)];
    header.view = _header;
    [_menu addItem:header];

    _permissionItem = [self action:@"Grant Input Monitoring…"
                          selector:@selector(showOnboarding:)
                               key:@""];
    _permissionItem.image = [NSImage imageWithSystemSymbolName:@"exclamationmark.circle"
                                      accessibilityDescription:@"Needs attention"];
    [_menu addItem:_permissionItem];
    _shoutTripItem = [self action:@"Shout Mode turned itself off…"
                         selector:@selector(showLabs:)
                              key:@""];
    _shoutTripItem.image = [NSImage imageWithSystemSymbolName:@"exclamationmark.circle"
                                     accessibilityDescription:@"Needs attention"];
    _shoutTripItem.toolTip = @"macOS kept pausing the keyboard tap, so Keyfeel stopped adding "
                             @"Shift. Open Labs to turn it on again.";
    [_menu addItem:_shoutTripItem];

    NSMenuItem *volume = [NSMenuItem new];
    _volume = [KFVolumeMenuView new];
    __weak KFStatusMenuController *weakSelf = self;
    _volume.onChange = ^(float value) {
        KFStatusMenuController *strongSelf = weakSelf;
        if (strongSelf != nil) {
            strongSelf->_context.preferences.volume = value;
        }
    };
    volume.view = _volume;
    [_menu addItem:volume];
    _micItem = Info(@"Muted while the microphone is in use");
    [_menu addItem:_micItem];

    [_menu addItem:NSMenuItem.separatorItem];
    [_menu addItem:Info(@"Sound Pack")];
    _packsEnd = [self action:@"More Packs…" selector:@selector(showPacks:) key:@""];
    [_menu addItem:_packsEnd];

    [_menu addItem:NSMenuItem.separatorItem];
    _forceItem = Info(@"");
    [_menu addItem:_forceItem];
    _lastForceItem = Info(@"");
    _keysItem = Info(@"");
    _tapsItem = Info(@"");
    [_menu addItem:_lastForceItem];
    [_menu addItem:_keysItem];
    [_menu addItem:_tapsItem];
    _badgeItem = Info(@"");
    [_menu addItem:_badgeItem];

    [_menu addItem:NSMenuItem.separatorItem];
    NSMenuItem *labs = [[NSMenuItem alloc] initWithTitle:@"Labs" action:nil keyEquivalent:@""];
    NSMenu *labsMenu = [[NSMenu alloc] initWithTitle:@"Labs"];
    _bigEnterItem = [self action:@"Big Enter" selector:@selector(toggleLab:) key:@""];
    _shoutItem = [self action:@"Shout Mode" selector:@selector(toggleLab:) key:@""];
    _meterItem = [self action:@"Force Meter" selector:@selector(toggleLab:) key:@""];
    _bigEnterItem.toolTip = @"Slamming Enter adds a deep boom.";
    _shoutItem.toolTip = @"Typing letters hard types them as capitals. Needs Accessibility.";
    _meterItem.toolTip = @"A small floating meter shows each keystroke's force.";
    _shoutStatusItem = Info(@"");
    _shoutStatusItem.indentationLevel = 1;
    for (NSMenuItem *lab in @[ _bigEnterItem, _shoutItem, _shoutStatusItem, _meterItem ]) {
        [labsMenu addItem:lab];
    }
    [labsMenu addItem:NSMenuItem.separatorItem];
    [labsMenu addItem:[self action:@"About Labs…" selector:@selector(showLabs:) key:@""]];
    labs.submenu = labsMenu;
    [_menu addItem:labs];

    [_menu addItem:NSMenuItem.separatorItem];
    [_menu addItem:[self action:@"Calibrate…" selector:@selector(showCalibration:) key:@""]];
    [_menu addItem:[self action:@"Settings…" selector:@selector(showSettings:) key:@","]];
    [_menu addItem:[self action:@"Check for Updates…"
                       selector:@selector(checkForUpdates:)
                            key:@""]];
    [_menu addItem:NSMenuItem.separatorItem];
    [_menu addItem:[self action:@"Send Feedback…" selector:@selector(sendFeedback:) key:@""]];
    NSMenuItem *quit = [[NSMenuItem alloc] initWithTitle:@"Quit Keyfeel"
                                                  action:@selector(terminate:)
                                           keyEquivalent:@"q"];
    quit.target = NSApp;
    [_menu addItem:quit];
}

- (void)rebuildPacks {
    for (NSMenuItem *item in _packItems) {
        [_menu removeItem:item];
    }
    [_packItems removeAllObjects];
    NSInteger index = [_menu indexOfItem:_packsEnd];
    NSArray<KFPackManifest *> *packs = _context.library.packs;
    if (packs.count == 0) {
        NSMenuItem *none = Info(@"No packs installed");
        [_menu insertItem:none atIndex:index];
        [_packItems addObject:none];
        return;
    }
    for (KFPackManifest *pack in packs) {
        NSMenuItem *item = [self action:pack.name selector:@selector(choosePack:) key:@""];
        item.representedObject = pack.identifier;
        item.indentationLevel = 1;
        [_menu insertItem:item atIndex:index++];
        [_packItems addObject:item];
    }
    [self refreshPacks];
}

#pragma mark - Refreshing

- (void)refreshIcon {
    const KFPowerState state = _context.power.state;
    const BOOL attention =
        state == KFPowerStateNoPermission || _context.power.shoutStatus == KFShoutStatusTripped;
    const KFStatusIconState icon = attention                       ? KFStatusIconAttention
                                   : state == KFPowerStateDisabled ? KFStatusIconOff
                                                                   : KFStatusIconOn;
    _item.button.image = [KFStatusIcon imageForState:icon];
    _item.button.accessibilityLabel = attention                       ? @"Keyfeel, needs attention"
                                      : state == KFPowerStateDisabled ? @"Keyfeel, sounds off"
                                                                      : @"Keyfeel";
}

- (nullable NSString *)shoutStatusText {
    switch (_context.power.shoutStatus) {
    case KFShoutStatusNeedsAccessibility:
        return @"Waiting for Accessibility";
    case KFShoutStatusUnavailable:
        return @"No force detection on this Mac";
    case KFShoutStatusPaused:
        return @"Paused while sounds are off";
    case KFShoutStatusTripped:
        return @"Turned off: the keyboard tap was paused";
    case KFShoutStatusOff:
    case KFShoutStatusOn:
        return nil;
    }
    return nil;
}

- (void)refreshPacks {
    NSString *current = [_context.library resolvePack:_context.preferences.packId].identifier;
    for (NSMenuItem *item in _packItems) {
        item.state = [item.representedObject isEqualToString:current] ? NSControlStateValueOn
                                                                      : NSControlStateValueOff;
    }
}

- (NSString *)forceText {
    switch (_context.power.forceStatus) {
    case KFForceStatusLive:
        return @"Force: Live";
    case KFForceStatusExternalKeyboard:
        return @"Force: Off (external keyboard)";
    case KFForceStatusUnavailable:
        return @"Force: Not available on this Mac";
    case KFForceStatusDisabled:
        return @"Force: Enable in Settings";
    case KFForceStatusNeedsCalibration:
        return @"Force: Calibrate for best results";
    }
    return @"";
}

- (void)refreshLive {
    const float velocity = kf_sensor_last_velocity();
    const BOOL measured = _context.power.forceStatus == KFForceStatusLive ||
                          _context.power.forceStatus == KFForceStatusNeedsCalibration;
    _lastForceItem.title =
        measured && velocity >= 0.0f
            ? [NSString stringWithFormat:@"Last keystroke: %.0f%% force", (double)velocity * 100.0]
            : @"Last keystroke: —";
    [_context.stats update]; /* In memory: the file is written at most every 5 minutes. */
    _keysItem.title =
        [NSString stringWithFormat:@"Keys today: %@",
                                   [NSNumberFormatter
                                       localizedStringFromNumber:@(_context.stats.today.keys)
                                                     numberStyle:NSNumberFormatterDecimalStyle]];
    _tapsItem.title = [NSString
        stringWithFormat:@"Taps per minute: %lu", (unsigned long)_context.stats.tapsPerMinute];
    NSString *badge = _context.badgeLine;
    _badgeItem.hidden = badge == nil;
    _badgeItem.title = badge ?: @"";
}

- (void)refreshAll {
    KFPreferences *prefs = _context.preferences;
    _header.toggle.state = prefs.enabled ? NSControlStateValueOn : NSControlStateValueOff;
    _volume.volume = prefs.volume;
    _permissionItem.hidden = _context.power.state != KFPowerStateNoPermission;
    _shoutTripItem.hidden = _context.power.shoutStatus != KFShoutStatusTripped;
    NSString *shout = [self shoutStatusText];
    _shoutStatusItem.hidden = shout == nil;
    _shoutStatusItem.title = shout ?: @"";
    _micItem.hidden = !_context.power.microphoneMuted;
    _forceItem.title = [self forceText];
    _bigEnterItem.state = prefs.labsBigEnter ? NSControlStateValueOn : NSControlStateValueOff;
    _shoutItem.state = prefs.labsShoutMode ? NSControlStateValueOn : NSControlStateValueOff;
    _meterItem.state = prefs.labsForceMeter ? NSControlStateValueOn : NSControlStateValueOff;
    [self refreshPacks];
    [self refreshLive];
    [self refreshIcon];
}

- (void)stateChanged:(NSNotification *)note {
    (void)note;
    [self refreshIcon];
    if (_liveTimer != nil) {
        [self refreshAll];
    }
}

- (void)packsChanged:(NSNotification *)note {
    (void)note;
    [self rebuildPacks];
}

#pragma mark - Status item

- (void)statusItemClicked:(NSStatusBarButton *)sender {
    NSEvent *event = NSApp.currentEvent;
    const BOOL menu = event.type == NSEventTypeRightMouseUp ||
                      (event.modifierFlags & NSEventModifierFlagControl) != 0;
    if (menu) {
        [_panel close];
        _item.menu =
            _menu; // Shown at once by this click, then detached so left clicks open the panel.
        [sender performClick:nil];
        _item.menu = nil;
    } else {
        [_panel toggleBelowButton:sender];
    }
}

- (BOOL)renderPanelPreviewToDirectory:(NSURL *)directory {
    return [_panel renderPreviewToDirectory:directory];
}

#pragma mark - NSMenuDelegate

- (void)menuWillOpen:(NSMenu *)menu {
    if (menu != _menu) {
        return;
    }
    [_context.power checkPermission];
    [self refreshAll];
    __weak KFStatusMenuController *weakSelf = self;
    _liveTimer = [NSTimer timerWithTimeInterval:kLiveRefresh
                                        repeats:YES
                                          block:^(NSTimer *timer) {
                                              (void)timer;
                                              [weakSelf refreshLive];
                                          }];
    _liveTimer.tolerance = 0.1;
    /* Menus track in their own run-loop mode: common modes keep the timer firing. */
    [NSRunLoop.mainRunLoop addTimer:_liveTimer forMode:NSRunLoopCommonModes];
}

- (void)menuDidClose:(NSMenu *)menu {
    if (menu == _menu) {
        [_liveTimer invalidate];
        _liveTimer = nil;
        [_context.power acknowledgeShoutTrip]; /* Seen: the icon's dot can go. */
    }
}

#pragma mark - Actions

- (void)toggleEnabled:(NSSwitch *)sender {
    _context.preferences.enabled = sender.state == NSControlStateValueOn;
}

- (void)choosePack:(NSMenuItem *)sender {
    _context.preferences.packId = sender.representedObject;
    [self refreshPacks];
}

- (void)toggleLab:(NSMenuItem *)sender {
    KFPreferences *prefs = _context.preferences;
    if (sender == _bigEnterItem) {
        prefs.labsBigEnter = !prefs.labsBigEnter;
    } else if (sender == _shoutItem) {
        prefs.labsShoutMode = !prefs.labsShoutMode;
    } else if (sender == _meterItem) {
        prefs.labsForceMeter = !prefs.labsForceMeter;
    }
    [self refreshAll];
}

- (void)showOnboarding:(id)sender {
    (void)sender;
    [_context.actions showOnboarding];
}

- (void)showPacks:(id)sender {
    (void)sender;
    [_context.actions showSettingsTab:@"packs"];
}

- (void)showLabs:(id)sender {
    (void)sender;
    [_context.actions showSettingsTab:@"labs"];
}

- (void)showSettings:(id)sender {
    (void)sender;
    [_context.actions showSettingsTab:@"general"];
}

- (void)showCalibration:(id)sender {
    (void)sender;
    [_context.actions showCalibration];
}

- (void)checkForUpdates:(id)sender {
    (void)sender;
    [_context.actions checkForUpdates];
}

- (void)sendFeedback:(id)sender {
    (void)sender;
    [_context.actions sendFeedback];
}

@end
