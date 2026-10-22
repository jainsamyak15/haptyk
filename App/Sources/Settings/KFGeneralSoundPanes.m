// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "Settings/KFSettingsPanes.h"

#import <ServiceManagement/ServiceManagement.h>

#import "Packs/KFPackLibrary.h"
#import "Power/KFPowerController.h"
#import "Preferences/KFPreferences.h"
#import "Support/KFLog.h"
#import "Support/KFUI.h"

static const CGFloat kPaneWidth = 560.0;

@implementation KFSettingsPane

- (instancetype)initWithContext:(KFAppContext *)context {
    if ((self = [super initWithNibName:nil bundle:nil])) {
        _context = context;
    }
    return self;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    for (NSNotificationName name in @[
             KFPreferencesDidChangeNotification, KFPackLibraryDidChangeNotification,
             KFPowerStateDidChangeNotification
         ]) {
        [NSNotificationCenter.defaultCenter addObserver:self
                                               selector:@selector(changed:)
                                                   name:name
                                                 object:nil];
    }
    [self refresh];
}

- (void)dealloc {
    [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)changed:(NSNotification *)note {
    (void)note;
    [self refresh];
}

- (void)refresh {
}

- (void)present:(NSAlert *)alert then:(nullable void (^)(NSModalResponse response))done {
    NSWindow *window = self.view.window;
    if (window != nil) {
        [alert beginSheetModalForWindow:window completionHandler:done];
    } else {
        const NSModalResponse response = [alert runModal];
        if (done != nil) {
            done(response);
        }
    }
}

@end

#pragma mark - General

@implementation KFGeneralPane {
    NSButton *_login;
    NSTextField *_loginNote;
    NSButton *_release;
    NSSlider *_width;
    NSButton *_repeat;
    NSButton *_mic;
    NSButton *_external;
    NSPopUpButton *_idle;
}

- (void)loadView {
    self.title = @"General";
    _login = KFCheckbox(@"Open Keyfeel at login", self, @selector(toggleLogin:));
    _loginNote = KFSecondaryLabel(@"");
    _release = KFCheckbox(@"Play a sound when a key is released", self, @selector(changeSetting:));
    _width = KFSlider(0.0, 1.0, 0.6, self, @selector(changeSetting:), @"Stereo width");
    _repeat = KFCheckbox(@"Play sounds for repeating keys", self, @selector(changeSetting:));
    _mic = KFCheckbox(@"Mute while the microphone is in use", self, @selector(changeSetting:));
    _external =
        KFCheckbox(@"Mute when typing on an external keyboard", self, @selector(changeSetting:));
    _idle = [NSPopUpButton new];
    for (NSNumber *seconds in @[ @15, @30, @60, @120, @300 ]) {
        [_idle addItemWithTitle:[NSString stringWithFormat:@"After %@ s without typing", seconds]];
        _idle.lastItem.tag = seconds.integerValue;
    }
    _idle.target = self;
    _idle.action = @selector(changeSetting:);
    _idle.accessibilityLabel = @"Sleep after no typing"; /* Its row labels the stack. */
    NSTextField *idleNote = KFSecondaryLabel(
        @"Audio and the motion sensor sleep when you stop typing, so Keyfeel uses no power while "
        @"idle. The first key after a pause plays a moment late.");
    self.view = KFPane(
        @[
            KFFormRow(@"Startup:", KFVStack(@[ _login, _loginNote ], 4.0)),
            KFFormRow(@"Keys:", KFVStack(@[ _release, _repeat ], 6.0)),
            KFFormRow(@"Stereo width:", KFVStack(
                                            @[
                                                _width, KFSecondaryLabel(@"Left keys sound from "
                                                                         @"the left, right keys "
                                                                         @"from the right.")
                                            ],
                                            4.0)),
            KFFormRow(@"Mute:", KFVStack(@[ _mic, _external ], 6.0)),
            KFFormRow(@"Sleep (advanced):", KFVStack(@[ _idle, idleNote ], 4.0)),
        ],
        kPaneWidth);
}

- (void)refresh {
    KFPreferences *prefs = self.context.preferences;
    _release.state = prefs.releaseSounds;
    _width.doubleValue = prefs.stereoWidth;
    _repeat.state = prefs.repeatSounds;
    _mic.state = prefs.muteWhenMicActive;
    _external.state = prefs.muteExternalKeyboard;
    if (![_idle selectItemWithTag:prefs.idleTimeoutS]) {
        [_idle selectItemWithTag:30];
    }
    const SMAppServiceStatus status = SMAppService.mainAppService.status;
    _login.state =
        status == SMAppServiceStatusEnabled ? NSControlStateValueOn : NSControlStateValueOff;
    _loginNote.stringValue = status == SMAppServiceStatusRequiresApproval
                                 ? @"Approve Keyfeel in System Settings > General > Login Items."
                                 : @"Off by default.";
}

- (void)changeSetting:(id)sender {
    KFPreferences *prefs = self.context.preferences;
    if (sender == _release) {
        prefs.releaseSounds = _release.state == NSControlStateValueOn;
    } else if (sender == _width) {
        prefs.stereoWidth = (float)_width.doubleValue;
    } else if (sender == _repeat) {
        prefs.repeatSounds = _repeat.state == NSControlStateValueOn;
    } else if (sender == _mic) {
        prefs.muteWhenMicActive = _mic.state == NSControlStateValueOn;
    } else if (sender == _external) {
        prefs.muteExternalKeyboard = _external.state == NSControlStateValueOn;
    } else if (sender == _idle) {
        prefs.idleTimeoutS = _idle.selectedTag;
    }
}

/// Launch at login only through SMAppService, only when the user asks (spec: Safety rules).
- (void)toggleLogin:(NSButton *)sender {
    NSError *error = nil;
    SMAppService *service = SMAppService.mainAppService;
    const BOOL ok = sender.state == NSControlStateValueOn
                        ? [service registerAndReturnError:&error]
                        : [service unregisterAndReturnError:&error];
    if (!ok) {
        os_log_error(KFLogApp(), "login item: %{public}@", error.localizedDescription);
        if (service.status == SMAppServiceStatusRequiresApproval) {
            [SMAppService openSystemSettingsLoginItems];
        }
    }
    [self refresh];
}

@end

#pragma mark - Sound

@implementation KFSoundPane {
    NSSlider *_volume;
    NSPopUpButton *_pack;
    NSSlider *_sensitivity;
    NSButton *_adapt;
    NSButton *_calibrate;
    NSTextField *_force;
}

- (void)loadView {
    self.title = @"Sound";
    _volume = KFSlider(0.0, 1.0, 0.7, self, @selector(changeSetting:), @"Volume");
    _pack = [NSPopUpButton new];
    _pack.target = self;
    _pack.action = @selector(changeSetting:);
    _pack.accessibilityLabel = @"Sound pack";
    _sensitivity = KFSlider(-1.0, 1.0, 0.0, self, @selector(changeSetting:), @"Sensitivity");
    _adapt = KFCheckbox(@"Adapt to my typing over time", self, @selector(changeSetting:));
    _calibrate = KFPushButton(@"Run Calibration…", self, @selector(calibrate:));
    _force = KFSecondaryLabel(@"");
    self.view = KFPane(
        @[
            KFFormRow(@"Volume:", _volume),
            KFFormRow(@"Sound pack:", _pack),
            KFFormRow(@"Sensitivity:",
                      KFVStack(
                          @[
                              _sensitivity, KFSecondaryLabel(@"Left: harder hits needed for "
                                                             @"loud sounds. Right: lighter.")
                          ],
                          4.0)),
            KFFormRow(@"Force:", KFVStack(@[ _force, _adapt, _calibrate ], 8.0)),
        ],
        kPaneWidth);
}

- (void)refresh {
    KFPreferences *prefs = self.context.preferences;
    _volume.doubleValue = prefs.volume;
    [_pack removeAllItems];
    KFPackManifest *current = [self.context.library resolvePack:prefs.packId];
    for (KFPackManifest *pack in self.context.library.packs) {
        [_pack addItemWithTitle:pack.name];
        _pack.lastItem.representedObject = pack.identifier;
        if (pack == current) {
            [_pack selectItem:_pack.lastItem];
        }
    }
    _pack.enabled = _pack.numberOfItems > 0;
    const BOOL force = self.context.power.forceStatus != KFForceStatusUnavailable;
    _sensitivity.doubleValue = prefs.sensitivity;
    _sensitivity.enabled = force;
    _adapt.state = prefs.autoAdapt;
    _adapt.enabled = force;
    _calibrate.enabled = force;
    _force.stringValue = force ? @"Measured with the motion sensor."
                               : @"Not available on this Mac: every key plays at the same "
                                 @"strength.";
}

- (void)changeSetting:(id)sender {
    KFPreferences *prefs = self.context.preferences;
    if (sender == _volume) {
        prefs.volume = (float)_volume.doubleValue;
    } else if (sender == _pack) {
        prefs.packId = _pack.selectedItem.representedObject;
    } else if (sender == _sensitivity) {
        prefs.sensitivity = (float)_sensitivity.doubleValue;
    } else if (sender == _adapt) {
        prefs.autoAdapt = _adapt.state == NSControlStateValueOn;
    }
}

- (void)calibrate:(id)sender {
    (void)sender;
    [self.context.actions showCalibration];
}

@end
