// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "KFAppTest.h"

#include <math.h>

#include <kf/kf_config.h>
#include <kf/kf_engine.h>
#include <kf/kf_pack.h>
#include <kf/kf_types.h>

#import "Labs/KFForceMeterController.h"
#import "Menu/KFFeelView.h"
#import "Menu/KFPackListView.h"
#import "Menu/KFPanelControls.h"
#import "Packs/KFPackLibrary.h"
#import "Packs/KFPackManifest.h"
#import "Power/KFPowerStateMachine.h"
#import "Preferences/KFPreferences.h"
#import "Stats/KFStatsController.h"
#import "Support/KFTheme.h"

#pragma mark - Power

void TestPowerStateMachine(void) {
    KFPowerAction a = KFPowerActionNone;
    /* DISABLED -> on -> IDLE: start the tap. */
    KFExpect([KFPowerStateMachine nextState:KFPowerStateDisabled
                                      event:KFPowerEventEnable
                                    actions:&a] == KFPowerStateIdle);
    KFExpect(a == KFPowerActionStartTap);
    /* IDLE -> key -> ACTIVE: audio and sensor, idle timer. */
    KFExpect([KFPowerStateMachine nextState:KFPowerStateIdle
                                      event:KFPowerEventKeyDown
                                    actions:&a] == KFPowerStateActive);
    KFExpect(a == (KFPowerActionStartAudio | KFPowerActionArmIdleTimer));
    /* Keys while ACTIVE do nothing: the timer checks the last key time. */
    KFExpect([KFPowerStateMachine nextState:KFPowerStateActive
                                      event:KFPowerEventKeyDown
                                    actions:&a] == KFPowerStateActive);
    KFExpect(a == KFPowerActionNone);
    /* ACTIVE -> idle timeout or sleep -> IDLE: everything but the tap stops. */
    for (int i = 0; i < 2; ++i) {
        const KFPowerEvent e = i == 0 ? KFPowerEventIdleTimeout : KFPowerEventSleep;
        KFExpect([KFPowerStateMachine nextState:KFPowerStateActive event:e
                                        actions:&a] == KFPowerStateIdle);
        KFExpect(a ==
                 (KFPowerActionStopAudio | KFPowerActionCancelIdleTimer | KFPowerActionFlushStats));
    }
    KFExpect([KFPowerStateMachine nextState:KFPowerStateIdle event:KFPowerEventSleep
                                    actions:&a] == KFPowerStateIdle);
    KFExpect(a == KFPowerActionNone);
    /* Output change: restart audio only while ACTIVE. */
    KFExpect([KFPowerStateMachine nextState:KFPowerStateActive
                                      event:KFPowerEventOutputChanged
                                    actions:&a] == KFPowerStateActive);
    KFExpect(a == KFPowerActionRestartAudio);
    KFExpect([KFPowerStateMachine nextState:KFPowerStateIdle
                                      event:KFPowerEventOutputChanged
                                    actions:&a] == KFPowerStateIdle);
    KFExpect(a == KFPowerActionNone);
    /* Any -> off -> DISABLED, stopping exactly what runs. */
    KFExpect([KFPowerStateMachine nextState:KFPowerStateActive
                                      event:KFPowerEventDisable
                                    actions:&a] == KFPowerStateDisabled);
    KFExpect((a & KFPowerActionStopAudio) && (a & KFPowerActionStopTap) &&
             (a & KFPowerActionCancelIdleTimer) && (a & KFPowerActionFlushStats));
    KFExpect([KFPowerStateMachine nextState:KFPowerStateIdle
                                      event:KFPowerEventDisable
                                    actions:&a] == KFPowerStateDisabled);
    KFExpect(a == KFPowerActionStopTap);
    /* Permission lost from anywhere but DISABLED: stop, show the badge. */
    KFExpect([KFPowerStateMachine nextState:KFPowerStateActive
                                      event:KFPowerEventPermissionLost
                                    actions:&a] == KFPowerStateNoPermission);
    KFExpect((a & KFPowerActionShowPermissionBadge) && (a & KFPowerActionStopAudio));
    KFExpect([KFPowerStateMachine nextState:KFPowerStateDisabled
                                      event:KFPowerEventPermissionLost
                                    actions:&a] == KFPowerStateDisabled);
    /* NO_PERMISSION -> granted -> IDLE; keys cannot arrive there. */
    KFExpect([KFPowerStateMachine nextState:KFPowerStateNoPermission
                                      event:KFPowerEventPermissionGranted
                                    actions:&a] == KFPowerStateIdle);
    KFExpect(a == (KFPowerActionHidePermissionBadge | KFPowerActionStartTap));
    KFExpect([KFPowerStateMachine nextState:KFPowerStateNoPermission
                                      event:KFPowerEventKeyDown
                                    actions:&a] == KFPowerStateNoPermission);
    KFExpect([KFPowerStateMachine nextState:KFPowerStateNoPermission
                                      event:KFPowerEventDisable
                                    actions:&a] == KFPowerStateDisabled);
    KFExpect(a == KFPowerActionHidePermissionBadge);
    KFExpect([[KFPowerStateMachine nameOfState:KFPowerStateActive] isEqualToString:@"active"]);
}

#pragma mark - Packs

static const char *kAudioBytes = "RIFF";

/// A pack folder with a pack.json and the files it names (one real WAV per file).
static NSURL *MakePack(NSDictionary *manifest, NSArray<NSString *> *files) {
    NSURL *root = [KFTestTemporaryFolder() URLByAppendingPathComponent:@"Test.kfpack"];
    [NSFileManager.defaultManager createDirectoryAtURL:[root URLByAppendingPathComponent:@"samples"]
                           withIntermediateDirectories:YES
                                            attributes:nil
                                                 error:NULL];
    NSData *json = [NSJSONSerialization dataWithJSONObject:manifest options:0 error:NULL];
    [json writeToURL:[root URLByAppendingPathComponent:@"pack.json"] atomically:YES];
    /* A 10 ms 16-bit mono 48 kHz WAV. */
    NSMutableData *wav = [NSMutableData data];
    const uint32_t frames = 480, data = frames * 2;
    const uint32_t header[] = {0x46464952, 36 + data, 0x45564157, 0x20746d66, 16};
    [wav appendBytes:header length:sizeof header];
    const uint16_t fmt[] = {1, 1};
    [wav appendBytes:fmt length:sizeof fmt];
    const uint32_t rates[] = {48000, 96000};
    [wav appendBytes:rates length:sizeof rates];
    const uint16_t block[] = {2, 16};
    [wav appendBytes:block length:sizeof block];
    const uint32_t chunk[] = {0x61746164, data};
    [wav appendBytes:chunk length:sizeof chunk];
    [wav increaseLengthBy:data];
    (void)kAudioBytes;
    for (NSString *file in files) {
        [wav writeToURL:[root URLByAppendingPathComponent:file] atomically:YES];
    }
    return root;
}

static NSMutableDictionary *ValidManifest(void) {
    return [@{
        @"format" : @1,
        @"id" : @"com.example.test",
        @"name" : @"Test",
        @"switch_type" : @"linear",
        @"author" : @"Keyfeel",
        @"license" : @"CC0",
        @"version" : @"1.0.0",
        @"source_url" : NSNull.null,
        @"attribution" : @"",
        @"derived" : @[ @"alpha/slam" ],
        @"groups" : @{
            @"alpha" : @{
                @"down" : @{@"soft" : @[ @"samples/a.wav" ], @"slam" : @[ @"samples/b.wav" ]},
                @"up" : @[ @"samples/u.wav" ]
            },
            @"space" : @{@"down" : @{@"hard" : @[ @"samples/a.wav" ]}},
        },
    } mutableCopy];
}

void TestPackManifestValid(void) {
    NSURL *root =
        MakePack(ValidManifest(), @[ @"samples/a.wav", @"samples/b.wav", @"samples/u.wav" ]);
    NSError *error = nil;
    KFPackManifest *pack = [KFPackManifest manifestWithPackAtURL:root error:&error];
    KFExpect(pack != nil);
    KFExpect([pack.identifier isEqualToString:@"com.example.test"]);
    KFExpect([pack.switchType isEqualToString:@"linear"]);
    KFExpect(pack.fileCount == 4); /* a.wav is listed twice. */
    KFExpect(pack.hasDerivedTiers);
    KFExpect(pack.sourceURL == nil && pack.attribution.length == 0);
    kf_pack *decoded = [pack loadPackAtSampleRate:48000.0 error:&error];
    KFExpect(decoded != NULL);
    if (decoded != NULL) {
        const kf_pack_sound *takes = NULL;
        KFExpect(kf_pack_sounds(decoded, KF_GROUP_ALPHA, false, KF_TIER_MEDIUM, &takes) == 1);
        KFExpect(kf_pack_is_derived(decoded, KF_GROUP_ALPHA, KF_TIER_SLAM));
        kf_pack_free(decoded);
    }
}

static BOOL Rejects(NSDictionary *manifest, NSArray<NSString *> *files, KFPackError code) {
    NSError *error = nil;
    KFPackManifest *pack = [KFPackManifest manifestWithPackAtURL:MakePack(manifest, files)
                                                           error:&error];
    return pack == nil && error.code == code && error.localizedDescription.length > 0;
}

void TestPackManifestRejectsBadInput(void) {
    NSArray *files = @[ @"samples/a.wav", @"samples/b.wav", @"samples/u.wav" ];
    NSMutableDictionary *m;

    m = ValidManifest();
    m[@"format"] = @2;
    KFExpect(Rejects(m, files, KFPackErrorMalformed));
    m = ValidManifest();
    m[@"id"] = @"no-dots";
    KFExpect(Rejects(m, files, KFPackErrorMalformed));
    m = ValidManifest();
    m[@"switch_type"] = @"mushy";
    KFExpect(Rejects(m, files, KFPackErrorMalformed));
    m = ValidManifest();
    [m removeObjectForKey:@"name"];
    KFExpect(Rejects(m, files, KFPackErrorMalformed));
    m = ValidManifest();
    m[@"source_url"] = @"ftp://example.com/x";
    KFExpect(Rejects(m, files, KFPackErrorMalformed));
    m = ValidManifest();
    m[@"derived"] = @[ @"alpha/loud" ];
    KFExpect(Rejects(m, files, KFPackErrorMalformed));
    m = ValidManifest();
    m[@"groups"] = @{@"alpha" : @{@"down" : @{@"thunder" : @[ @"samples/a.wav" ]}}};
    KFExpect(Rejects(m, files, KFPackErrorMalformed)); /* Unknown tier. */
    m = ValidManifest();
    m[@"groups"] = @{@"emoji" : @{@"down" : @{@"soft" : @[ @"samples/a.wav" ]}}};
    KFExpect(Rejects(m, files, KFPackErrorMalformed)); /* Unknown group. */
    m = ValidManifest();
    m[@"groups"] = @{@"space" : @{@"down" : @{@"soft" : @[ @"samples/a.wav" ]}}};
    KFExpect(Rejects(m, files, KFPackErrorMalformed)); /* No alpha key-down. */

    /* Paths: traversal, absolute, dot segments, wrong type, missing. */
    NSArray<NSString *> *unsafe =
        @[ @"../escape.wav", @"/etc/passwd.wav", @"samples/./a.wav", @"samples//a.wav" ];
    for (NSString *path in unsafe) {
        m = ValidManifest();
        m[@"groups"] = @{@"alpha" : @{@"down" : @{@"soft" : @[ path ]}}};
        KFExpect(Rejects(m, files, KFPackErrorUnsafePath));
    }
    m = ValidManifest();
    m[@"groups"] = @{@"alpha" : @{@"down" : @{@"soft" : @[ @"samples/a.mp3" ]}}};
    KFExpect(Rejects(m, files, KFPackErrorMalformed));
    m = ValidManifest();
    m[@"groups"] = @{@"alpha" : @{@"down" : @{@"soft" : @[ @"samples/missing.wav" ]}}};
    KFExpect(Rejects(m, files, KFPackErrorMissingFile));

    /* A symbolic link anywhere on the path is refused. */
    NSURL *root = MakePack(ValidManifest(), files);
    NSURL *link = [root URLByAppendingPathComponent:@"samples/link.wav"];
    [NSFileManager.defaultManager
        createSymbolicLinkAtURL:link
             withDestinationURL:[root URLByAppendingPathComponent:@"samples/a.wav"]
                          error:NULL];
    m = ValidManifest();
    m[@"groups"] = @{@"alpha" : @{@"down" : @{@"soft" : @[ @"samples/link.wav" ]}}};
    [[NSJSONSerialization dataWithJSONObject:m options:0 error:NULL]
        writeToURL:[root URLByAppendingPathComponent:@"pack.json"]
        atomically:YES];
    NSError *error = nil;
    KFExpect([KFPackManifest manifestWithPackAtURL:root error:&error] == nil &&
             error.code == KFPackErrorUnsafePath);

    /* Not JSON at all, and no pack.json. */
    [@"{not json" writeToURL:[root URLByAppendingPathComponent:@"pack.json"]
                  atomically:YES
                    encoding:NSUTF8StringEncoding
                       error:NULL];
    KFExpect([KFPackManifest manifestWithPackAtURL:root error:&error] == nil &&
             error.code == KFPackErrorMalformed);
    [NSFileManager.defaultManager removeItemAtURL:[root URLByAppendingPathComponent:@"pack.json"]
                                            error:NULL];
    KFExpect([KFPackManifest manifestWithPackAtURL:root error:&error] == nil &&
             error.code == KFPackErrorUnreadable);
}

void TestPackLibraryOrderAndDefault(void) {
    NSURL *bundled = KFTestTemporaryFolder();
    NSArray *specs = @[
        @[ @"com.example.nk", @"NK Cream", @"linear" ],
        @[ @"com.example.alpaca", @"Alpaca", @"linear" ],
        @[ @"com.example.box", @"Kailh Box Navy", @"clicky" ]
    ];
    for (NSArray *spec in specs) {
        NSMutableDictionary *m = ValidManifest();
        m[@"id"] = spec[0];
        m[@"name"] = spec[1];
        m[@"switch_type"] = spec[2];
        NSURL *pack = MakePack(m, @[ @"samples/a.wav", @"samples/b.wav", @"samples/u.wav" ]);
        [NSFileManager.defaultManager
            moveItemAtURL:pack
                    toURL:[bundled
                              URLByAppendingPathComponent:[spec[0]
                                                              stringByAppendingString:@".kfpack"]]
                    error:NULL];
    }
    KFPackLibrary *library = [[KFPackLibrary alloc] initWithBundledURL:bundled
                                                               userURL:KFTestTemporaryFolder()];
    NSArray *names = [library.packs valueForKey:@"name"];
    KFExpect(
        [names isEqualToArray:(@[ @"Alpaca", @"Kailh Box Navy", @"NK Cream" ])]); /* Spec order. */
    KFExpect([[library resolvePack:nil].name isEqualToString:@"Alpaca"]); /* First linear. */
    KFExpect([[library resolvePack:@"com.example.box"].name isEqualToString:@"Kailh Box Navy"]);
    KFExpect([[library resolvePack:@"com.example.gone"].name isEqualToString:@"Alpaca"]);
    KFExpect([library isBundled:library.packs.firstObject]);
    NSError *error = nil;
    KFExpect(![library deletePack:library.packs.firstObject error:&error]); /* Bundled: never. */
}

#pragma mark - Labs

void TestLabsBoomAndMeter(void) {
    /* Force Meter: the text and the nearest recorded tier. */
    KFExpect([[KFForceMeterController textForVelocity:-1.0f] isEqualToString:@"Not measured"]);
    KFExpect([[KFForceMeterController textForVelocity:NAN] isEqualToString:@"Not measured"]);
    KFExpect([[KFForceMeterController textForVelocity:0.0f] isEqualToString:@"0% · Soft"]);
    KFExpect([[KFForceMeterController textForVelocity:0.72f] isEqualToString:@"72% · Hard"]);
    KFExpect([[KFForceMeterController textForVelocity:7.0f] isEqualToString:@"100% · Slam"]);
    KFExpect([[KFForceMeterController tierNameForVelocity:0.16f] isEqualToString:@"Soft"]);
    KFExpect([[KFForceMeterController tierNameForVelocity:0.17f] isEqualToString:@"Medium"]);
    KFExpect([[KFForceMeterController tierNameForVelocity:0.84f] isEqualToString:@"Slam"]);

    /* Big Enter: the boom joins the pack only while set, and a change means a reload. */
    KFExpect(kf_engine_init() == KF_OK);
    NSURL *bundled = KFTestTemporaryFolder();
    NSURL *pack =
        MakePack(ValidManifest(), @[ @"samples/a.wav", @"samples/b.wav", @"samples/u.wav" ]);
    [NSFileManager.defaultManager moveItemAtURL:pack
                                          toURL:[bundled URLByAppendingPathComponent:@"t.kfpack"]
                                          error:NULL];
    KFPackLibrary *library = [[KFPackLibrary alloc] initWithBundledURL:bundled
                                                               userURL:KFTestTemporaryFolder()];
    KFPackManifest *manifest = library.packs.firstObject;
    KFExpect(manifest != nil);
    if (manifest == nil) {
        return;
    }
    NSError *error = nil;
    KFExpect([library needsLoad:manifest rate:48000.0]);
    KFExpect([library loadPackNow:manifest rate:48000.0 error:&error]);
    KFExpect(![library needsLoad:manifest rate:48000.0]);
    KFExpect([library needsLoad:manifest rate:44100.0]);

    NSURL *boom = [NSURL fileURLWithPath:@"App/Resources/Labs/boom.wav"];
    library.boomURL = boom;
    KFExpect([library needsLoad:manifest rate:48000.0]);
    KFExpect([library loadPackNow:manifest rate:48000.0 error:&error]);
    KFExpect(![library needsLoad:manifest rate:48000.0]);
    kf_pack *decoded = [manifest loadPackAtSampleRate:48000.0 boom:boom error:&error];
    const kf_pack_sound *takes = NULL;
    KFExpect(decoded != NULL &&
             kf_pack_sounds(decoded, KF_SOUND_BOOM, false, KF_TIER_SOFT, &takes) == 1u &&
             takes[0].frames == 43200u); /* 0.9 s at 48 kHz. */
    kf_pack_free(decoded);
    decoded = [manifest loadPackAtSampleRate:44100.0 boom:boom error:&error];
    KFExpect(decoded != NULL &&
             kf_pack_sounds(decoded, KF_SOUND_BOOM, false, KF_TIER_SOFT, &takes) == 1u &&
             takes[0].frames >= 39690u && takes[0].frames <= 39700u); /* Resampled. */
    kf_pack_free(decoded);

    /* A boom that fails to decode never costs the key sounds. */
    library.boomURL = [bundled URLByAppendingPathComponent:@"missing.wav"];
    KFExpect([library loadPackNow:manifest rate:48000.0 error:&error]);
    KFExpect(library.boomURL == nil && ![library needsLoad:manifest rate:48000.0]);
    kf_engine_set_pack(NULL);
}

#pragma mark - Status panel

static NSColor *Resolved(NSColor *color, NSAppearanceName name) {
    __block NSColor *out = nil;
    [[NSAppearance appearanceNamed:name] performAsCurrentDrawingAppearance:^{
        out = [color colorUsingColorSpace:NSColorSpace.sRGBColorSpace];
    }];
    return out ?: NSColor.clearColor;
}

void TestStatusPanelPieces(void) {
    /* Activity: one count per second, oldest first, only the last `count` seconds. */
    kf_live_stats live;
    memset(&live, 0, sizeof live);
    const uint64_t s = 1000000000ull;
    for (int i = 0; i < 3; ++i) {
        kf_live_stats_note_key(&live, 100 * s + 1);
    }
    kf_live_stats_note_key(&live, 98 * s);
    kf_live_stats_note_key(&live, 20 * s); // Older than a minute: not shown.
    uint32_t seconds[60];
    kf_live_stats_seconds(&live, 100 * s + 5, seconds, 60);
    KFExpect(seconds[59] == 3 && seconds[57] == 1 && seconds[58] == 0);
    uint32_t total = 0;
    for (int i = 0; i < 60; ++i) {
        total += seconds[i];
    }
    KFExpect(total == 4);
    uint32_t few[5];
    kf_live_stats_seconds(&live, 100 * s + 5, few, 5);
    KFExpect(few[4] == 3 && few[2] == 1);
    uint32_t big[60];
    kf_live_stats_seconds(&live, 100 * s, big, 200); // Clamped to 60.
    KFExpect(big[59] == 3);

    /* Pack filters. */
    KFExpect([KFPackListView typesForFilter:@"All"] == nil);
    KFExpect([[KFPackListView typesForFilter:@"Clicky"] isEqualToArray:@[ @"clicky" ]]);
    KFExpect([[KFPackListView typesForFilter:@"Other"] containsObject:@"topre"]);
    KFExpect([[KFPackListView typesForFilter:@"Other"] containsObject:@"buckling"]);

    /* Theme: light and dark differ; each switch type has its own colour. */
    NSColor *light = Resolved(KFTheme.background, NSAppearanceNameAqua);
    NSColor *dark = Resolved(KFTheme.background, NSAppearanceNameDarkAqua);
    KFExpect(light.brightnessComponent > 0.9 && dark.brightnessComponent < 0.15);
    NSColor *accent = Resolved(KFTheme.accent, NSAppearanceNameDarkAqua);
    KFExpect(accent.redComponent > 0.9 && accent.blueComponent < 0.3); // Orange, never purple.
    NSMutableSet<NSString *> *colours = [NSMutableSet set];
    for (NSString *type in KFPackManifest.switchTypes) {
        NSColor *c = Resolved([KFTheme colorForSwitchType:type], NSAppearanceNameDarkAqua);
        [colours addObject:[NSString stringWithFormat:@"%.2f%.2f%.2f", c.redComponent,
                                                      c.greenComponent, c.blueComponent]];
    }
    KFExpect(colours.count == KFPackManifest.switchTypes.count);

    /* Feel card and tiles speak to VoiceOver. */
    KFFeelView *feel = [KFFeelView new];
    [feel setVelocity:0.72f status:@"Force: Live"];
    KFExpect([feel.accessibilityLabel containsString:@"Hard"]);
    [feel setVelocity:-1.0f status:@"Force: Not on this Mac"];
    KFExpect([feel.accessibilityLabel containsString:@"not measured"]);
    KFStatTile *tile = [[KFStatTile alloc] initWithLabel:@"Keys today"];
    tile.value = @"42";
    KFExpect([tile.accessibilityLabel isEqualToString:@"Keys today: 42"]);
    KFPillSwitch *pill = [KFPillSwitch new];
    pill.on = YES;
    KFExpect([pill.accessibilityValue isEqual:@1] && pill.state == NSControlStateValueOn);

    /* Glass: clear panel, translucent surfaces, white accent in dark, orange only as the brand;
     * larger radii. Graphite again afterwards. */
    const KFThemeStyle saved = KFTheme.style;
    KFTheme.style = KFThemeStyleGlass;
    KFExpect(KFTheme.glass);
    KFExpect(Resolved(KFTheme.background, NSAppearanceNameDarkAqua).alphaComponent == 0.0);
    const CGFloat surfaceAlpha = Resolved(KFTheme.surface, NSAppearanceNameDarkAqua).alphaComponent;
    KFExpect(surfaceAlpha > 0.0 && surfaceAlpha < 0.5);
    NSColor *white = Resolved(KFTheme.accent, NSAppearanceNameDarkAqua);
    KFExpect(white.redComponent > 0.99 && white.greenComponent > 0.99 &&
             white.blueComponent > 0.99);
    NSColor *brand = Resolved(KFTheme.brand, NSAppearanceNameDarkAqua);
    KFExpect(brand.redComponent > 0.9 && brand.blueComponent < 0.3);
    KFExpect([KFTheme radius:10.0] > 10.0);
    KFTheme.style = KFThemeStyleGraphite;
    KFExpect([KFTheme radius:10.0] == 10.0);
    KFExpect(Resolved(KFTheme.background, NSAppearanceNameDarkAqua).alphaComponent == 1.0);
    KFTheme.style = saved;
}

#pragma mark - Preferences and stats

void TestPreferencesDefaultsAndClamping(void) {
    NSString *suite = [@"com.keyfeel.tests." stringByAppendingString:NSUUID.UUID.UUIDString];
    NSUserDefaults *defaults = [[NSUserDefaults alloc] initWithSuiteName:suite];
    KFPreferences *prefs = [[KFPreferences alloc] initWithDefaults:defaults];
    KFExpect(prefs.enabled && prefs.releaseSounds && prefs.muteWhenMicActive && prefs.autoAdapt);
    KFExpect(!prefs.repeatSounds && !prefs.muteExternalKeyboard && !prefs.telemetryOptIn);
    KFExpect(!prefs.labsBigEnter && !prefs.labsShoutMode && !prefs.labsForceMeter);
    KFExpect(prefs.idleTimeoutS == 30 && prefs.packId == nil && prefs.calibration == nil);
    KFExpect([prefs.panelStyle isEqualToString:@"graphite"]);
    prefs.panelStyle = @"glass";
    KFExpect([prefs.panelStyle isEqualToString:@"glass"]);
    prefs.panelStyle = @"neon"; // Unknown: the default.
    KFExpect([prefs.panelStyle isEqualToString:@"graphite"]);
    prefs.volume = 3.0f;
    KFExpect(prefs.volume == 1.0f);
    prefs.sensitivity = -9.0f;
    KFExpect(prefs.sensitivity == -1.0f);
    prefs.idleTimeoutS = 1;
    KFExpect(prefs.idleTimeoutS == 5);
    prefs.calibration = @{@"soft_g" : @0.01, @"slam_g" : @0.2};
    KFExpect([prefs.calibration[@"slam_g"] doubleValue] == 0.2);
    NSString *first = prefs.installId;
    KFExpect(first.length == 36 && [prefs.installId isEqualToString:first]);
    [prefs regenerateInstallId];
    KFExpect(![prefs.installId isEqualToString:first]);
    [defaults removePersistentDomainForName:suite];
}

void TestStatsBucketsFlushAndReset(void) {
    KFExpect([KFStatsController bucketForPeakG:0.0f] == 0);
    KFExpect([KFStatsController bucketForPeakG:KF_STATS_FORCE_MIN_G * 0.5f] == 0);
    KFExpect([KFStatsController bucketForPeakG:KF_STATS_FORCE_MAX_G * 10.0f] ==
             KF_STATS_FORCE_BUCKETS - 1);
    NSUInteger last = 0;
    for (float g = KF_STATS_FORCE_MIN_G * 1.01f; g < KF_STATS_FORCE_MAX_G; g *= 1.3f) {
        const NSUInteger bucket = [KFStatsController bucketForPeakG:g];
        KFExpect(bucket >= last && bucket < KF_STATS_FORCE_BUCKETS); /* Monotonic. */
        last = bucket;
    }

    NSURL *folder = KFTestTemporaryFolder();
    KFStatsController *stats = [[KFStatsController alloc] initWithDirectory:folder];
    for (int i = 0; i < 25; ++i) {
        kf_live_stats_note_key(stats.live, (uint64_t)i * 100000000ull + 5000000000ull);
    }
    [stats notePeakG:0.05f];
    [stats flush];
    KFExpect(stats.today.keys == 25);
    NSString *file = [stats.today.day stringByAppendingPathExtension:@"json"];
    NSData *saved = [NSData dataWithContentsOfURL:[folder URLByAppendingPathComponent:file]];
    NSDictionary *json =
        saved != nil ? [NSJSONSerialization JSONObjectWithData:saved options:0 error:NULL] : nil;
    KFExpect([json[@"keys"] integerValue] == 25);
    KFExpect([json[@"force_hist"] count] == KF_STATS_FORCE_BUCKETS);
    KFExpect(json[@"day"] != nil && json.count == 5); /* Counts only: nothing else is stored. */

    KFStatsController *again = [[KFStatsController alloc] initWithDirectory:folder];
    KFExpect(again.today.keys == 25); /* Today's file is read back. */
    [again reset];
    KFExpect(again.today.keys == 0);
    KFExpect(
        [NSFileManager.defaultManager contentsOfDirectoryAtPath:folder.path error:NULL].count == 0);
}

#pragma mark - M5: badges, telemetry, diagnostics

#import "Application/KFAppContext.h"
#import "Stats/KFBadge.h"
#import "Telemetry/KFDiagnostics.h"
#import "Telemetry/KFTelemetry.h"

static NSArray<NSNumber *> *Histogram(NSUInteger bucket, NSUInteger count) {
    NSMutableArray<NSNumber *> *h = [NSMutableArray array];
    for (NSUInteger i = 0; i < KF_STATS_FORCE_BUCKETS; ++i) {
        [h addObject:@(i == bucket ? count : 0)];
    }
    return h;
}

void TestBadgeMath(void) {
    /* All keystrokes in one bucket: the median sits inside that bucket's edges. */
    const double lo = log((double)KF_STATS_FORCE_MIN_G);
    const double step = (log((double)KF_STATS_FORCE_MAX_G) - lo) / KF_STATS_FORCE_BUCKETS;
    const double median = [KFBadge medianForceOfHistogram:Histogram(3, 100)];
    KFExpect(median > exp(lo + 3 * step) && median < exp(lo + 4 * step));
    KFExpect(isnan([KFBadge medianForceOfHistogram:Histogram(0, 0)]));
    NSArray<NSNumber *> *tooShort = @[ @1, @2 ];
    const double shortMedian = [KFBadge medianForceOfHistogram:tooShort];
    KFExpect(isnan(shortMedian));

    NSMutableArray<NSNumber *> *table = [NSMutableArray array];
    for (int p = 0; p <= 100; ++p) {
        [table addObject:@(0.01 * (p + 1))]; /* Force rises linearly with the percentile. */
    }
    KFExpect(fabs([KFBadge percentileOfForce:0.51 table:table] - 50.0) < 1e-9);
    KFExpect(fabs([KFBadge percentileOfForce:0.515 table:table] - 50.5) < 1e-9);
    KFExpect([KFBadge percentileOfForce:0.001 table:table] == 0.0);
    KFExpect([KFBadge percentileOfForce:9.0 table:table] == 100.0);
    NSArray<NSNumber *> *descending = @[ @2, @1 ];
    const double unordered = [KFBadge percentileOfForce:0.5 table:descending];
    KFExpect(isnan(unordered)); /* Not ascending. */
    KFExpect(isnan([KFBadge percentileOfForce:NAN table:table]));

    KFExpect([KFBadge badgeForPercentile:0] == KFBadgeFeather);
    KFExpect([KFBadge badgeForPercentile:24.9] == KFBadgeFeather);
    KFExpect([KFBadge badgeForPercentile:25] == KFBadgeSteady);
    KFExpect([KFBadge badgeForPercentile:59.9] == KFBadgeSteady);
    KFExpect([KFBadge badgeForPercentile:60] == KFBadgeHammer);
    KFExpect([KFBadge badgeForPercentile:89.9] == KFBadgeHammer);
    KFExpect([KFBadge badgeForPercentile:90] == KFBadgeThunder);
    KFExpect([[KFBadge nameOfBadge:KFBadgeThunder] isEqualToString:@"Thunder"]);
    KFExpect(KFBadge.requiredKeystrokes == 500);

    NSData *png = [KFBadge cardWithBadge:KFBadgeHammer percentile:72 packName:@"Test"];
    NSBitmapImageRep *rep = png != nil ? [NSBitmapImageRep imageRepWithData:png] : nil;
    KFExpect(rep != nil && rep.pixelsWide == 1200 && rep.pixelsHigh == 630);
}

void TestTelemetryPayloadAndSafety(void) {
    KFExpect(KFTelemetry.baseURL == nil); /* No backend configured: nothing ever goes out. */
    KFDayStats *day = [KFDayStats new];
    day.day = @"2026-10-21";
    day.keys = 4812;
    day.forceHistogram = Histogram(4, 10);
    NSDictionary *payload = [KFTelemetry payloadForDay:day installId:@"ID" forceMode:@"fallback"];
    NSSet *keys = [NSSet setWithArray:payload.allKeys];
    NSSet *allowed = [NSSet setWithArray:@[
        @"install_id", @"app_version", @"mac_model", @"macos", @"day", @"keys", @"force_hist",
        @"force_mode"
    ]];
    KFExpect([keys isEqualToSet:allowed]); /* Exactly the spec's fields: counts only. */
    KFExpect([payload[@"keys"] integerValue] == 4812 &&
             [payload[@"day"] isEqualToString:@"2026-10-21"]);
    KFExpect([NSJSONSerialization isValidJSONObject:payload]);
}

void TestDiagnosticsReports(void) {
    NSURL *folder = KFTestTemporaryFolder();
    KFDiagnostics *diagnostics = [[KFDiagnostics alloc] initWithDirectory:folder];
    KFExpect(diagnostics.reportCount == 0);
    [diagnostics saveReport:[@"{\"crash\":1}" dataUsingEncoding:NSUTF8StringEncoding]];
    [diagnostics saveReport:[@"{\"hang\":2}" dataUsingEncoding:NSUTF8StringEncoding]];
    KFExpect(diagnostics.reportCount == 2);
    KFExpect(diagnostics.unsentReports.count == 2);
    [diagnostics markAllSent];
    KFExpect(diagnostics.unsentReports.count == 0 && diagnostics.reportCount == 2);

    NSString *suite = [@"com.keyfeel.tests." stringByAppendingString:NSUUID.UUID.UUIDString];
    NSUserDefaults *defaults = [[NSUserDefaults alloc] initWithSuiteName:suite];
    KFAppContext *context = [KFAppContext new];
    context.preferences = [[KFPreferences alloc] initWithDefaults:defaults];
    NSString *report = [diagnostics reportForContext:context];
    KFExpect([report containsString:@"Keyfeel diagnostics"]);
    KFExpect([report containsString:@"Crash and hang reports kept: 2"]);
    KFExpect([report containsString:@"No key data"]);
    [defaults removePersistentDomainForName:suite];
}
