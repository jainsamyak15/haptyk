// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "Settings/KFSettingsWindowController.h"

#import "Settings/KFSettingsPanes.h"

@implementation KFSettingsWindowController {
    NSTabViewController *_tabs;
    NSArray<NSString *> *_identifiers;
}

- (instancetype)initWithContext:(KFAppContext *)context {
    NSTabViewController *tabs = [NSTabViewController new];
    tabs.tabStyle = NSTabViewControllerTabStyleToolbar;
    const struct {
        __unsafe_unretained NSString *identifier;
        __unsafe_unretained NSString *symbol;
        Class pane;
    } items[] = {
        {@"general", @"gearshape", KFGeneralPane.class},
        {@"sound", @"speaker.wave.2", KFSoundPane.class},
        {@"packs", @"square.stack.3d.up", KFPacksPane.class},
        {@"labs", @"flask", KFLabsPane.class},
        {@"stats", @"chart.bar", KFStatsPane.class},
        {@"about", @"info.circle", KFAboutPane.class},
    };
    NSMutableArray<NSString *> *identifiers = [NSMutableArray array];
    for (size_t i = 0; i < sizeof items / sizeof items[0]; ++i) {
        KFSettingsPane *pane = [[items[i].pane alloc] initWithContext:context];
        NSTabViewItem *item = [NSTabViewItem tabViewItemWithViewController:pane];
        item.identifier = items[i].identifier;
        (void)pane.view; /* Loads the view, which sets the title. */
        item.label = pane.title ?: items[i].identifier;
        item.image = [NSImage imageWithSystemSymbolName:items[i].symbol
                               accessibilityDescription:item.label];
        [tabs addTabViewItem:item];
        [identifiers addObject:items[i].identifier];
    }
    NSWindow *window = [NSWindow windowWithContentViewController:tabs];
    window.styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable;
    window.releasedWhenClosed = NO;
    if ((self = [super initWithWindow:window])) {
        _tabs = tabs;
        _identifiers = identifiers;
        window.delegate = self;
        window.title = @"Keyfeel Settings";
    }
    return self;
}

- (void)selectTab:(NSString *)identifier {
    const NSUInteger index = [_identifiers indexOfObject:identifier];
    _tabs.selectedTabViewItemIndex = index != NSNotFound ? (NSInteger)index : 0;
    [self.window center];
    [NSApp activateIgnoringOtherApps:YES];
    [self showWindow:nil];
}

- (void)windowWillClose:(NSNotification *)notification {
    (void)notification;
    if (self.onClose != nil) {
        self.onClose(); /* The app drops its reference: windows live only while open. */
    }
}

@end
