// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "Settings/KFSettingsPanes.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <kf/kf_types.h>

#import "Packs/KFPackLibrary.h"
#import "Power/KFPowerController.h"
#import "Preferences/KFPreferences.h"
#import "Support/KFUI.h"

static NSString *const kNameColumn = @"name";
static NSString *const kTypeColumn = @"type";
static NSString *const kSourceColumn = @"source";

@interface KFPacksPane () <NSTableViewDataSource, NSTableViewDelegate>
@end

@implementation KFPacksPane {
    NSTableView *_table;
    NSButton *_import;
    NSButton *_reveal;
    NSButton *_delete;
    NSButton *_preview;
    NSTextField *_detail;
}

- (void)loadView {
    self.title = @"Packs";
    _table = [NSTableView new];
    _table.usesAlternatingRowBackgroundColors = YES;
    _table.dataSource = self;
    _table.delegate = self;
    _table.accessibilityLabel = @"Sound packs";
    const struct {
        NSString *identifier;
        NSString *title;
        CGFloat width;
    } columns[] = {{kNameColumn, @"Name", 240},
                   {kTypeColumn, @"Switch type", 110},
                   {kSourceColumn, @"Source", 110}};
    for (size_t i = 0; i < sizeof columns / sizeof columns[0]; ++i) {
        NSTableColumn *column = [[NSTableColumn alloc] initWithIdentifier:columns[i].identifier];
        column.title = columns[i].title;
        column.width = columns[i].width;
        [_table addTableColumn:column];
    }
    NSScrollView *scroll = [NSScrollView new];
    scroll.documentView = _table;
    scroll.hasVerticalScroller = YES;
    scroll.borderType = NSBezelBorder;
    [scroll.heightAnchor constraintEqualToConstant:220].active = YES;
    [scroll.widthAnchor constraintEqualToConstant:520].active = YES;

    _import = KFPushButton(@"Import…", self, @selector(importPack:));
    _reveal = KFPushButton(@"Show in Finder", self, @selector(reveal:));
    _delete = KFPushButton(@"Delete", self, @selector(deletePack:));
    _preview = KFPushButton(@"Preview", self, @selector(preview:));
    _preview.toolTip = @"Plays the soft, medium, hard and slam sounds";
    _detail = KFSecondaryLabel(@"");
    NSTextField *note = KFSecondaryLabel(
        @"Import a .kfpack folder or a .zip of one. Everything is checked before it is copied, "
        @"and nothing in a pack is ever run.");
    self.view = KFPane(
        @[ scroll, KFHStack(@[ _import, _reveal, _delete, _preview ], 8.0), _detail, note ], 560.0);
}

- (nullable KFPackManifest *)selectedPack {
    const NSInteger row = _table.selectedRow;
    NSArray<KFPackManifest *> *packs = self.context.library.packs;
    return row >= 0 && row < (NSInteger)packs.count ? packs[(NSUInteger)row] : nil;
}

- (void)refresh {
    KFPackManifest *selected = self.selectedPack;
    [_table reloadData];
    NSArray<KFPackManifest *> *packs = self.context.library.packs;
    KFPackManifest *target =
        selected ?: [self.context.library resolvePack:self.context.preferences.packId];
    const NSUInteger index = target != nil ? [packs indexOfObject:target] : NSNotFound;
    if (index != NSNotFound) {
        [_table selectRowIndexes:[NSIndexSet indexSetWithIndex:index] byExtendingSelection:NO];
    }
    [self updateButtons];
}

- (void)updateButtons {
    KFPackManifest *pack = self.selectedPack;
    _reveal.enabled = pack != nil;
    _preview.enabled = pack != nil;
    _delete.enabled = pack != nil && ![self.context.library isBundled:pack];
    _detail.stringValue =
        pack == nil
            ? @""
            : [NSString stringWithFormat:@"%@ %@ by %@ · %@%@", pack.name, pack.version,
                                         pack.author, pack.license,
                                         pack.hasDerivedTiers ? @" · some tiers generated" : @""];
}

#pragma mark - Table

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView {
    (void)tableView;
    return (NSInteger)self.context.library.packs.count;
}

- (nullable NSView *)tableView:(NSTableView *)tableView
            viewForTableColumn:(nullable NSTableColumn *)column
                           row:(NSInteger)row {
    (void)tableView;
    KFPackManifest *pack = self.context.library.packs[(NSUInteger)row];
    NSString *text = [column.identifier isEqualToString:kNameColumn] ? pack.name
                     : [column.identifier isEqualToString:kTypeColumn]
                         ? pack.switchType.capitalizedString
                         : ([self.context.library isBundled:pack] ? @"Bundled" : @"Imported");
    NSTextField *cell = [NSTextField labelWithString:text];
    cell.lineBreakMode = NSLineBreakByTruncatingTail;
    return cell;
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification {
    (void)notification;
    [self updateButtons];
}

#pragma mark - Actions

- (void)alert:(NSString *)title detail:(NSString *)detail {
    NSAlert *alert = [NSAlert new];
    alert.messageText = title;
    alert.informativeText = detail;
    [self present:alert then:nil];
}

- (void)importPack:(id)sender {
    (void)sender;
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.canChooseDirectories = YES;
    panel.canChooseFiles = YES;
    panel.allowsMultipleSelection = NO;
    panel.allowedContentTypes = @[ UTTypeFolder, UTTypeZIP ];
    panel.message = @"Choose a .kfpack folder or a .zip file";
    [panel beginWithCompletionHandler:^(NSModalResponse result) {
        NSURL *chosen = panel.URL;
        if (result != NSModalResponseOK || chosen == nil) {
            return;
        }
        self->_import.enabled = NO;
        [self.context.library
            importPackAtURL:chosen
                 completion:^(KFPackManifest *pack, NSError *error) {
                     self->_import.enabled = YES;
                     if (pack == nil) {
                         [self alert:@"The pack was not imported"
                              detail:error.localizedDescription ?: @"Unknown error."];
                         return;
                     }
                     self.context.preferences.packId = pack.identifier;
                     [self refresh];
                 }];
    }];
}

- (void)reveal:(id)sender {
    (void)sender;
    KFPackManifest *pack = self.selectedPack;
    if (pack != nil) {
        [NSWorkspace.sharedWorkspace activateFileViewerSelectingURLs:@[ pack.packURL ]];
    }
}

- (void)deletePack:(id)sender {
    (void)sender;
    KFPackManifest *pack = self.selectedPack;
    if (pack == nil) {
        return;
    }
    NSAlert *alert = [NSAlert new];
    alert.messageText = [NSString stringWithFormat:@"Delete \"%@\"?", pack.name];
    alert.informativeText = @"The imported pack is removed from this Mac.";
    [alert addButtonWithTitle:@"Delete"];
    [alert addButtonWithTitle:@"Cancel"];
    alert.buttons.firstObject.hasDestructiveAction = YES;
    [self present:alert
             then:^(NSModalResponse response) {
                 if (response != NSAlertFirstButtonReturn) {
                     return;
                 }
                 NSError *error = nil;
                 if (![self.context.library deletePack:pack error:&error]) {
                     [self alert:@"The pack was not deleted"
                          detail:error.localizedDescription ?: @""];
                 }
             }];
}

/// Makes the selected pack current, then plays soft, medium, hard and slam.
- (void)preview:(id)sender {
    (void)sender;
    KFPackManifest *pack = self.selectedPack;
    if (pack == nil) {
        return;
    }
    self.context.preferences.packId = pack.identifier;
    const float velocities[4] = {0.0f, 1.0f / 3.0f, 2.0f / 3.0f, 1.0f};
    for (int i = 0; i < 4; ++i) {
        const float v = velocities[i];
        dispatch_after(
            dispatch_time(DISPATCH_TIME_NOW, (int64_t)((0.15 + 0.35 * i) * NSEC_PER_SEC)),
            dispatch_get_main_queue(), ^{
                [self.context.power playTestKeyWithVelocity:v group:KF_GROUP_ALPHA];
            });
    }
}

@end
