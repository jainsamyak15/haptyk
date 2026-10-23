// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "Menu/KFPackListView.h"

#import "Menu/KFPanelControls.h"
#import "Packs/KFPackManifest.h"
#import "Support/KFTheme.h"

static const CGFloat kRowHeight = 30.0;
static NSUserInterfaceItemIdentifier const kCellID = @"KFPackCell";

/// Selection and hover drawn in the theme: a quiet accent wash and an accent bar.
@interface KFPackRowView : NSTableRowView
@property (nonatomic) BOOL current;
@end

@implementation KFPackRowView {
    BOOL _hovered;
}

- (void)updateTrackingAreas {
    [super updateTrackingAreas];
    for (NSTrackingArea *area in self.trackingAreas.copy) {
        [self removeTrackingArea:area];
    }
    [self addTrackingArea:[[NSTrackingArea alloc]
                              initWithRect:NSZeroRect
                                   options:NSTrackingMouseEnteredAndExited |
                                           NSTrackingActiveAlways | NSTrackingInVisibleRect
                                     owner:self
                                  userInfo:nil]];
}

- (void)mouseEntered:(NSEvent *)event {
    (void)event;
    _hovered = YES;
    [self setNeedsDisplay:YES];
}

- (void)mouseExited:(NSEvent *)event {
    (void)event;
    _hovered = NO;
    [self setNeedsDisplay:YES];
}

- (void)drawBackgroundInRect:(NSRect)dirty {
    (void)dirty;
    if (_hovered && !self.selected && !self.current) {
        [KFTheme.surfaceHover setFill];
        [[NSBezierPath bezierPathWithRoundedRect:NSInsetRect(self.bounds, 2.0, 1.0)
                                         xRadius:7.0
                                         yRadius:7.0] fill];
    }
}

- (void)drawSelectionInRect:(NSRect)dirty {
    (void)dirty;
    [self drawMarks];
}

- (void)drawRect:(NSRect)dirty {
    [super drawRect:dirty];
    if (self.current && !self.selected) {
        [self drawMarks];
    }
}

- (void)drawMarks {
    const NSRect row = NSInsetRect(self.bounds, 2.0, 1.0);
    [KFTheme.surfaceHover setFill]; // Neutral: the accent is kept for the bar and the check.
    [[NSBezierPath bezierPathWithRoundedRect:row xRadius:7.0 yRadius:7.0] fill];
    if (self.current) {
        [KFTheme.accent setFill];
        [[NSBezierPath bezierPathWithRoundedRect:NSMakeRect(NSMinX(row) + 2.0, NSMinY(row) + 7.0,
                                                            3.0, row.size.height - 14.0)
                                         xRadius:1.5
                                         yRadius:1.5] fill];
    }
}

- (NSTableViewSelectionHighlightStyle)selectionHighlightStyle {
    return NSTableViewSelectionHighlightStyleRegular; // Drawn by drawSelectionInRect:.
}

- (BOOL)isEmphasized {
    return NO;
}

@end

@interface KFPackCell : NSTableCellView
@property (nonatomic, readonly) NSTextField *name;
@property (nonatomic, readonly) KFTypeTag *typeTag;
@property (nonatomic, readonly) NSImageView *check;
@end

@implementation KFPackCell

- (instancetype)initWithFrame:(NSRect)frame {
    if ((self = [super initWithFrame:frame])) {
        self.identifier = kCellID;
        _name = KFThemedLabel(@"", [NSFont systemFontOfSize:13.0 weight:NSFontWeightMedium],
                              KFTheme.textPrimary);
        _typeTag = [KFTypeTag new];
        NSImage *mark = [NSImage imageWithSystemSymbolName:@"checkmark"
                                  accessibilityDescription:@"Current"]
                            ?: [[NSImage alloc] initWithSize:NSMakeSize(12, 12)];
        _check = [NSImageView imageViewWithImage:mark];
        _check.contentTintColor = KFTheme.accent;
        _check.symbolConfiguration =
            [NSImageSymbolConfiguration configurationWithPointSize:11.0 weight:NSFontWeightBold];
        for (NSView *v in @[ _name, _typeTag, _check ]) {
            v.translatesAutoresizingMaskIntoConstraints = NO;
            [self addSubview:v];
        }
        [_name setContentCompressionResistancePriority:NSLayoutPriorityDefaultLow
                                        forOrientation:NSLayoutConstraintOrientationHorizontal];
        [NSLayoutConstraint activateConstraints:@[
            [_name.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:14.0],
            [_name.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
            [_typeTag.leadingAnchor constraintGreaterThanOrEqualToAnchor:_name.trailingAnchor
                                                                constant:8.0],
            [_typeTag.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
            [_check.leadingAnchor constraintEqualToAnchor:_typeTag.trailingAnchor constant:8.0],
            [_check.trailingAnchor constraintEqualToAnchor:self.trailingAnchor constant:-10.0],
            [_check.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
            [_check.widthAnchor constraintEqualToConstant:12.0],
        ]];
        self.textField = _name;
    }
    return self;
}

@end

@interface KFPackListView () <NSTableViewDataSource, NSTableViewDelegate>
@end

@implementation KFPackListView {
    NSArray<KFChipButton *> *_chips;
    NSString *_filter;
    NSArray<KFPackManifest *> *_shown;
    NSScrollView *_scroll;
    NSTableView *_table;
    NSLayoutConstraint *_height;
    BOOL _reloading;
}

+ (nullable NSArray<NSString *> *)typesForFilter:(NSString *)filter {
    if ([filter isEqualToString:@"Linear"]) {
        return @[ @"linear" ];
    }
    if ([filter isEqualToString:@"Tactile"]) {
        return @[ @"tactile" ];
    }
    if ([filter isEqualToString:@"Clicky"]) {
        return @[ @"clicky" ];
    }
    if ([filter isEqualToString:@"Other"]) {
        return @[ @"topre", @"buckling", @"other" ];
    }
    return nil;
}

- (instancetype)initWithFrame:(NSRect)frame {
    if ((self = [super initWithFrame:frame])) {
        _packs = @[];
        _shown = @[];
        _filter = @"All";
        _visibleRows = 6;
        NSMutableArray<KFChipButton *> *chips = [NSMutableArray array];
        for (NSString *title in @[ @"All", @"Linear", @"Tactile", @"Clicky", @"Other" ]) {
            KFChipButton *chip = [KFChipButton chipWithTitle:title
                                                      target:self
                                                      action:@selector(filterChosen:)];
            chip.accessibilityLabel =
                [NSString stringWithFormat:@"Show %@ packs", title.lowercaseString];
            [chips addObject:chip];
        }
        _chips = chips;
        _chips.firstObject.on = YES;
        NSStackView *chipRow = [NSStackView stackViewWithViews:_chips];
        chipRow.spacing = 6.0;

        _table = [NSTableView new];
        NSTableColumn *column = [[NSTableColumn alloc] initWithIdentifier:@"pack"];
        [_table addTableColumn:column];
        _table.headerView = nil;
        _table.rowHeight = kRowHeight;
        _table.intercellSpacing = NSMakeSize(0, 0);
        _table.backgroundColor = NSColor.clearColor;
        _table.gridStyleMask = NSTableViewGridNone;
        _table.style = NSTableViewStylePlain;
        _table.dataSource = self;
        _table.delegate = self;
        _table.target = self;
        _table.action = @selector(rowClicked:);
        _table.accessibilityLabel = @"Sound packs";
        _table.focusRingType = NSFocusRingTypeNone;

        _scroll = [NSScrollView new];
        _scroll.documentView = _table;
        _scroll.drawsBackground = NO;
        _scroll.hasVerticalScroller = YES;
        _scroll.autohidesScrollers = YES;
        _scroll.scrollerStyle = NSScrollerStyleOverlay;
        _scroll.borderType = NSNoBorder;
        _scroll.automaticallyAdjustsContentInsets = NO;

        KFCardView *card = [KFCardView new];
        card.cornerRadius = 12.0;
        for (NSView *v in @[ chipRow, card, _scroll ]) {
            v.translatesAutoresizingMaskIntoConstraints = NO;
        }
        [self addSubview:chipRow];
        [self addSubview:card];
        [card addSubview:_scroll];
        _height =
            [_scroll.heightAnchor constraintEqualToConstant:kRowHeight * (CGFloat)_visibleRows];
        [NSLayoutConstraint activateConstraints:@[
            [chipRow.topAnchor constraintEqualToAnchor:self.topAnchor],
            [chipRow.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
            [card.topAnchor constraintEqualToAnchor:chipRow.bottomAnchor constant:8.0],
            [card.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
            [card.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
            [card.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
            [_scroll.topAnchor constraintEqualToAnchor:card.topAnchor constant:4.0],
            [_scroll.bottomAnchor constraintEqualToAnchor:card.bottomAnchor constant:-4.0],
            [_scroll.leadingAnchor constraintEqualToAnchor:card.leadingAnchor constant:2.0],
            [_scroll.trailingAnchor constraintEqualToAnchor:card.trailingAnchor constant:-2.0],
            _height,
        ]];
    }
    return self;
}

- (void)setVisibleRows:(NSUInteger)visibleRows {
    _visibleRows = visibleRows;
    _height.constant = kRowHeight * ((CGFloat)visibleRows + 0.5); // A half row hints at scrolling.
}

- (void)setPacks:(NSArray<KFPackManifest *> *)packs {
    _packs = [packs copy];
    [self reload];
}

- (void)setCurrentIdentifier:(nullable NSString *)currentIdentifier {
    _currentIdentifier = [currentIdentifier copy];
    [self reload];
}

- (void)reload {
    NSArray<NSString *> *types = [KFPackListView typesForFilter:_filter];
    NSMutableArray<KFPackManifest *> *shown = [NSMutableArray array];
    for (KFPackManifest *pack in _packs) {
        if (types == nil || [types containsObject:pack.switchType]) {
            [shown addObject:pack];
        }
    }
    _shown = shown;
    _reloading = YES;
    [_table reloadData];
    const NSUInteger row =
        [_shown indexOfObjectPassingTest:^BOOL(KFPackManifest *p, NSUInteger i, BOOL *stop) {
            (void)i;
            (void)stop;
            return [p.identifier isEqualToString:self.currentIdentifier ?: @""];
        }];
    if (row != NSNotFound) {
        [_table selectRowIndexes:[NSIndexSet indexSetWithIndex:row] byExtendingSelection:NO];
    } else {
        [_table deselectAll:nil];
    }
    _reloading = NO;
}

- (void)revealCurrent {
    if (_table.selectedRow >= 0) {
        [_table scrollRowToVisible:_table.selectedRow];
    }
}

- (void)filterChosen:(KFChipButton *)sender {
    for (KFChipButton *chip in _chips) {
        chip.on = chip == sender;
    }
    _filter = sender.title;
    [self reload];
    [self revealCurrent];
}

- (void)rowClicked:(id)sender {
    (void)sender;
    [self chooseRow:_table.clickedRow];
}

- (void)chooseRow:(NSInteger)row {
    if (row < 0 || (NSUInteger)row >= _shown.count) {
        return;
    }
    KFPackManifest *pack = _shown[(NSUInteger)row];
    if (self.onChoose != nil) {
        self.onChoose(pack);
    }
}

#pragma mark - Table

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView {
    (void)tableView;
    return (NSInteger)_shown.count;
}

- (nullable NSTableRowView *)tableView:(NSTableView *)tableView rowViewForRow:(NSInteger)row {
    (void)tableView;
    KFPackRowView *view = [KFPackRowView new];
    view.current =
        [_shown[(NSUInteger)row].identifier isEqualToString:self.currentIdentifier ?: @""];
    return view;
}

- (nullable NSView *)tableView:(NSTableView *)tableView
            viewForTableColumn:(nullable NSTableColumn *)column
                           row:(NSInteger)row {
    (void)column;
    KFPackCell *cell = [tableView makeViewWithIdentifier:kCellID owner:self] ?: [KFPackCell new];
    KFPackManifest *pack = _shown[(NSUInteger)row];
    const BOOL current = [pack.identifier isEqualToString:self.currentIdentifier ?: @""];
    cell.name.stringValue = pack.name;
    cell.name.textColor = current ? KFTheme.textPrimary : KFTheme.textSecondary;
    cell.typeTag.switchType = pack.switchType;
    cell.check.hidden = !current;
    cell.accessibilityLabel = [NSString stringWithFormat:@"%@, %@%@", pack.name, pack.switchType,
                                                         current ? @", current pack" : @""];
    return cell;
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification {
    (void)notification;
    if (_reloading) {
        return;
    }
    // Keyboard selection (arrow keys) chooses too, so the pack can be auditioned row by row.
    if (NSApp.currentEvent.type == NSEventTypeKeyDown) {
        [self chooseRow:_table.selectedRow];
    }
}

@end
