// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "Menu/KFStatusPanel.h"

#import <CoreImage/CoreImage.h>

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
static const CGFloat kGlassGap = 10.0;      // Between glass tiles, as in Control Center.
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

/// The panel's rounded background and hairline (Graphite); nothing in Glass, where the tiles
/// float over the desktop. A preview may draw a desktop backdrop instead.
@interface KFPanelBackground : NSView
@property (nonatomic, nullable) NSImage *backdrop;
@end

@implementation KFPanelBackground

- (void)drawRect:(NSRect)dirty {
    (void)dirty;
    if (self.backdrop != nil) {
        [self.backdrop drawInRect:self.bounds];
        return;
    }
    if (KFTheme.glass) {
        return;
    }
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
        [_text setContentCompressionResistancePriority:NSLayoutPriorityDefaultLow
                                        forOrientation:NSLayoutConstraintOrientationHorizontal];
        row.translatesAutoresizingMaskIntoConstraints = NO;
        [self addSubview:row];
        // Required padding (a stack's edge insets give way when a container compresses it).
        [NSLayoutConstraint activateConstraints:@[
            [row.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:10.0],
            [row.trailingAnchor constraintEqualToAnchor:self.trailingAnchor constant:-10.0],
            [row.topAnchor constraintEqualToAnchor:self.topAnchor constant:6.0],
            [row.bottomAnchor constraintEqualToAnchor:self.bottomAnchor constant:-6.0],
            [row.heightAnchor constraintGreaterThanOrEqualToConstant:22.0],
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

static BOOL DrawingDark(void) {
    NSAppearanceName match = [NSAppearance.currentDrawingAppearance
        bestMatchFromAppearancesWithNames:@[ NSAppearanceNameAqua, NSAppearanceNameDarkAqua ]];
    return [match isEqualToString:NSAppearanceNameDarkAqua];
}

/// The bright edge of a glass tile, for the system-blur fallback. Never takes clicks.
@interface KFGlassEdge : NSView
@property (nonatomic) CGFloat radius;
@end

@implementation KFGlassEdge

- (NSView *)hitTest:(NSPoint)point {
    (void)point;
    return nil;
}

- (void)drawRect:(NSRect)dirty {
    (void)dirty;
    NSBezierPath *shape = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(self.bounds, 0.5, 0.5)
                                                          xRadius:self.radius
                                                          yRadius:self.radius];
    [KFTheme.stroke setStroke];
    shape.lineWidth = 1.0;
    [shape stroke];
}

@end

/// A glass tile drawn from a blurred backdrop, for previews: an offscreen drawing cannot see what
/// is behind a window, which real glass and blur need.
@interface KFSimulatedGlass : NSView
@property (nonatomic) CGFloat radius;
@property (nonatomic, nullable) NSImage *backdrop; ///< Blurred, the size of the window.
@end

@implementation KFSimulatedGlass

- (void)drawRect:(NSRect)dirty {
    (void)dirty;
    NSBezierPath *shape = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(self.bounds, 0.5, 0.5)
                                                          xRadius:self.radius
                                                          yRadius:self.radius];
    [NSGraphicsContext saveGraphicsState];
    [shape addClip];
    const NSRect inWindow = [self convertRect:self.bounds toView:nil];
    [self.backdrop drawInRect:self.bounds
                     fromRect:inWindow
                    operation:NSCompositingOperationCopy
                     fraction:1.0
               respectFlipped:YES
                        hints:nil];
    const BOOL dark = DrawingDark();
    [[NSColor colorWithWhite:dark ? 0.0 : 1.0 alpha:dark ? 0.28 : 0.42] setFill];
    NSRectFillUsingOperation(self.bounds, NSCompositingOperationSourceOver);
    [[NSColor colorWithWhite:1.0 alpha:dark ? 0.07 : 0.0] setFill];
    NSRectFillUsingOperation(self.bounds, NSCompositingOperationSourceOver);
    [NSGraphicsContext restoreGraphicsState];
    [[NSColor colorWithWhite:1.0 alpha:dark ? 0.22 : 0.75] setStroke];
    shape.lineWidth = 1.0;
    [shape stroke];
}

@end

/// A resizable rounded-rectangle mask for NSVisualEffectView.
static NSImage *RoundedMask(CGFloat radius) {
    const CGFloat side = radius * 2.0 + 1.0;
    NSImage *mask = [NSImage imageWithSize:NSMakeSize(side, side)
                                   flipped:NO
                            drawingHandler:^BOOL(NSRect rect) {
                                [NSColor.blackColor setFill];
                                [[NSBezierPath bezierPathWithRoundedRect:rect
                                                                 xRadius:radius
                                                                 yRadius:radius] fill];
                                return YES;
                            }];
    mask.capInsets = NSEdgeInsetsMake(radius, radius, radius, radius);
    mask.resizingMode = NSImageResizingModeStretch;
    return mask;
}

/// The desktop picture's top-right corner (where the panel sits), `size` points at 2x; blurred
/// and saturated like a glass material when `blur`. Falls back to a neutral backdrop.
static NSImage *Backdrop(NSSize size, BOOL blur) {
    NSScreen *screen = NSScreen.mainScreen;
    NSURL *url =
        screen != nil ? [NSWorkspace.sharedWorkspace desktopImageURLForScreen:screen] : nil;
    NSImage *source = url != nil ? [[NSImage alloc] initWithContentsOfURL:url] : nil;
    CGImageRef cg = [source CGImageForProposedRect:NULL context:nil hints:nil];
    CIImage *image = cg != NULL ? [CIImage imageWithCGImage:cg] : nil;
    if (image == nil || CGRectIsEmpty(image.extent) || CGRectIsInfinite(image.extent)) {
        NSImage *neutral = [NSImage
             imageWithSize:NSMakeSize(400, 600)
                   flipped:NO
            drawingHandler:^BOOL(NSRect rect) {
                [[NSColor colorWithSRGBRed:0.13 green:0.15 blue:0.17 alpha:1] setFill];
                NSRectFill(rect);
                const struct {
                    CGFloat x, y, r, red, green, blue;
                } blobs[] = {{80, 450, 170, 0.18, 0.49, 0.48},
                             {320, 160, 200, 0.79, 0.55, 0.18},
                             {260, 520, 120, 0.23, 0.29, 0.35}};
                for (size_t i = 0; i < 3; ++i) {
                    [[NSColor colorWithSRGBRed:blobs[i].red
                                         green:blobs[i].green
                                          blue:blobs[i].blue
                                         alpha:1] setFill];
                    [[NSBezierPath
                        bezierPathWithOvalInRect:NSMakeRect(blobs[i].x - blobs[i].r,
                                                            blobs[i].y - blobs[i].r, blobs[i].r * 2,
                                                            blobs[i].r * 2)] fill];
                }
                return YES;
            }];
        CGImageRef fallback = [neutral CGImageForProposedRect:NULL context:nil hints:nil];
        if (fallback == NULL) {
            return [[NSImage alloc] initWithSize:size];
        }
        image = [[CIImage imageWithCGImage:fallback] imageByApplyingGaussianBlurWithSigma:40.0];
        image = [image imageByCroppingToRect:CGRectMake(0, 0, 400, 600)];
    }
    const CGRect extent = image.extent;
    const CGFloat aspect = size.width / size.height;
    CGFloat h = extent.size.height * 0.8;
    CGFloat w = h * aspect;
    if (w > extent.size.width) {
        w = extent.size.width;
        h = w / aspect;
    }
    const CGRect crop = CGRectMake(CGRectGetMaxX(extent) - w, CGRectGetMaxY(extent) - h, w, h);
    const CGFloat scale = size.width * 2.0 / w;
    image = [[image imageByCroppingToRect:crop]
        imageByApplyingTransform:CGAffineTransformConcat(CGAffineTransformMakeTranslation(
                                                             -crop.origin.x, -crop.origin.y),
                                                         CGAffineTransformMakeScale(scale, scale))];
    const CGRect target = CGRectMake(0, 0, size.width * 2.0, size.height * 2.0);
    if (blur) {
        image = [[image imageByClampingToExtent] imageByApplyingGaussianBlurWithSigma:36.0];
        image = [image imageByApplyingFilter:@"CIColorControls"
                         withInputParameters:@{
                             kCIInputSaturationKey : @1.5
                         }];
        image = [image imageByCroppingToRect:target];
    }
    CGImageRef rendered = [[CIContext context] createCGImage:image fromRect:target];
    if (rendered == NULL) {
        return [[NSImage alloc] initWithSize:size];
    }
    NSImage *out = [[NSImage alloc] initWithCGImage:rendered size:size];
    CGImageRelease(rendered);
    return out;
}

@interface KFStatusPanelController () <NSWindowDelegate>
@end

@implementation KFStatusPanelController {
    KFAppContext *_context;
    KFPanelWindow *_panel;
    NSStackView *_stack;
    KFPillSwitch *_power;
    NSTextField *_subtitle;
    KFBanner *_banner;
    NSView *_bannerTile; // The banner, or its glass tile.
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
    KFPanelBackground *_background;
    BOOL _simulateGlass; // Previews: tiles drawn from a backdrop instead of live glass.
    NSImage *_backdropBlur;
}

- (instancetype)initWithContext:(KFAppContext *)context {
    if ((self = [super init])) {
        _context = context;
        KFTheme.style = [context.preferences.panelStyle isEqualToString:@"glass"]
                            ? KFThemeStyleGlass
                            : KFThemeStyleGraphite;
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

/// Glass: `content` on its own glass tile, `inset` inside it; Graphite: `content` unchanged.
- (NSView *)tile:(NSView *)content inset:(CGFloat)inset radius:(CGFloat)radius {
    if (!KFTheme.glass) {
        return content;
    }
    NSView *inner = [NSView new];
    content.translatesAutoresizingMaskIntoConstraints = NO;
    [inner addSubview:content];
    [NSLayoutConstraint activateConstraints:@[
        [content.topAnchor constraintEqualToAnchor:inner.topAnchor constant:inset],
        [content.bottomAnchor constraintEqualToAnchor:inner.bottomAnchor constant:-inset],
        [content.leadingAnchor constraintEqualToAnchor:inner.leadingAnchor constant:inset + 2.0],
        [content.trailingAnchor constraintEqualToAnchor:inner.trailingAnchor constant:-inset - 2.0],
    ]];
    NSView *tile = nil;
    if (_simulateGlass) {
        KFSimulatedGlass *glass = [KFSimulatedGlass new];
        glass.radius = radius;
        glass.backdrop = _backdropBlur;
        tile = glass;
    } else if (@available(macOS 26.0, *)) {
        // The content goes inside the glass: only a glass view's content view is drawn above
        // it. A container lifts its glass above everything else, which it would blur.
        NSGlassEffectView *glass = [NSGlassEffectView new];
        glass.cornerRadius = radius;
        glass.style = NSGlassEffectViewStyleRegular;
        // A light tint keeps text legible over bright or busy desktops.
        glass.tintColor = [NSColor colorWithName:nil
                                 dynamicProvider:^NSColor *(NSAppearance *appearance) {
                                     NSAppearanceName m =
                                         [appearance bestMatchFromAppearancesWithNames:@[
                                             NSAppearanceNameAqua, NSAppearanceNameDarkAqua
                                         ]];
                                     return [m isEqualToString:NSAppearanceNameDarkAqua]
                                                ? [NSColor colorWithWhite:0.0 alpha:0.22]
                                                : [NSColor colorWithWhite:1.0 alpha:0.30];
                                 }];
        glass.contentView = inner;
        tile = glass;
    } else {
        NSVisualEffectView *blur = [NSVisualEffectView new];
        blur.material = NSVisualEffectMaterialPopover;
        blur.blendingMode = NSVisualEffectBlendingModeBehindWindow;
        blur.state = NSVisualEffectStateActive;
        blur.maskImage = RoundedMask(radius);
        KFGlassEdge *edge = [KFGlassEdge new];
        edge.radius = radius;
        edge.translatesAutoresizingMaskIntoConstraints = NO;
        [blur addSubview:edge];
        [NSLayoutConstraint activateConstraints:@[
            [edge.topAnchor constraintEqualToAnchor:blur.topAnchor],
            [edge.bottomAnchor constraintEqualToAnchor:blur.bottomAnchor],
            [edge.leadingAnchor constraintEqualToAnchor:blur.leadingAnchor],
            [edge.trailingAnchor constraintEqualToAnchor:blur.trailingAnchor],
        ]];
        tile = blur;
    }
    inner.translatesAutoresizingMaskIntoConstraints = NO;
    if (inner.superview == nil) { // Below the fallback's bright edge, which never takes clicks.
        [tile addSubview:inner positioned:NSWindowBelow relativeTo:nil];
    }
    [NSLayoutConstraint activateConstraints:@[
        [inner.topAnchor constraintEqualToAnchor:tile.topAnchor],
        [inner.bottomAnchor constraintEqualToAnchor:tile.bottomAnchor],
        [inner.leadingAnchor constraintEqualToAnchor:tile.leadingAnchor],
        [inner.trailingAnchor constraintEqualToAnchor:tile.trailingAnchor],
    ]];
    return tile;
}

- (NSView *)header {
    NSTextField *title = KFThemedLabel(
        @"Keyfeel", [NSFont systemFontOfSize:16.0 weight:NSFontWeightBold], KFTheme.textPrimary);
    NSTextField *dot =
        KFThemedLabel(@".", [NSFont systemFontOfSize:16.0 weight:NSFontWeightBold], KFTheme.brand);
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
    if (!KFTheme.glass) {
        return row;
    }
    // Glass: a titled module, as Control Center's Sound.
    NSTextField *title = KFThemedLabel(
        @"Volume", [NSFont systemFontOfSize:13.0 weight:NSFontWeightSemibold], KFTheme.textPrimary);
    title.accessibilityElement = NO;
    NSStackView *module = [NSStackView stackViewWithViews:@[ title, row ]];
    module.orientation = NSUserInterfaceLayoutOrientationVertical;
    module.alignment = NSLayoutAttributeLeading;
    module.spacing = 8.0;
    [row.widthAnchor constraintEqualToAnchor:module.widthAnchor].active = YES;
    return module;
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
    NSMutableArray<NSView *> *tiles = [NSMutableArray array];
    for (KFStatTile *tile in @[ _keysTile, _tapsTile, _thirdTile ]) {
        tile.glassBacked = KFTheme.glass;
        [tiles addObject:[self tile:tile inset:0.0 radius:18.0]];
    }
    NSStackView *row = [NSStackView stackViewWithViews:tiles];
    row.distribution = NSStackViewDistributionFillEqually;
    row.spacing = KFTheme.glass ? kGlassGap : 8.0;
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
    const BOOL glass = KFTheme.glass;
    _banner = [KFBanner new];
    _banner.link.target = self;
    _banner.link.action = @selector(bannerAction:);
    _banner.glassBacked = glass;
    _feel = [KFFeelView new];
    _feel.glassBacked = glass;
    NSView *packs = [self packSection];
    _packs.glassBacked = glass;
    NSArray<NSView *> *rows = @[
        [self tile:[self header] inset:12.0 radius:24.0],
        [self tile:_banner inset:0.0 radius:18.0],
        [self tile:_feel inset:0.0 radius:22.0],
        [self tile:[self volumeRow] inset:12.0 radius:24.0],
        [self tile:packs inset:12.0 radius:24.0],
        [self tiles],
        [self tile:[self labsRow] inset:12.0 radius:24.0],
        Hairline(),
        [self tile:[self footer] inset:9.0 radius:20.0],
    ];
    rows[7].hidden = glass; // Glass separates by tiles, not rules.
    // Glass: the banner's tile shows and hides with the banner.
    _bannerTile = rows[1];
    _stack = [NSStackView stackViewWithViews:rows];
    _stack.orientation = NSUserInterfaceLayoutOrientationVertical;
    _stack.alignment = NSLayoutAttributeLeading;
    _stack.spacing = glass ? kGlassGap : 14.0;
    if (!glass) {
        [_stack setCustomSpacing:12.0 afterView:rows[0]];
        [_stack setCustomSpacing:10.0 afterView:rows[7]];
    }
    const CGFloat pad = glass ? 8.0 : kPad; // Glass: room for the tiles' own edges.
    _stack.edgeInsets = glass ? NSEdgeInsetsMake(pad, pad, pad, pad)
                              : NSEdgeInsetsMake(kPad, kPad, kPad - 4.0, kPad);
    _stack.translatesAutoresizingMaskIntoConstraints = NO;
    for (NSView *row in rows) {
        [row.widthAnchor constraintEqualToConstant:kWidth - 2.0 * pad].active = YES;
    }
    KFPanelBackground *background =
        [[KFPanelBackground alloc] initWithFrame:NSMakeRect(0, 0, kWidth, 600)];
    _background = background;
    NSView *holder = _stack;
    BOOL liveGlass = NO;
    if (glass && !_simulateGlass) {
        if (@available(macOS 26.0, *)) {
            liveGlass = YES;
            // Renders the tiles' glass as one batch. Spacing 0: separate tiles, never merged.
            NSGlassEffectContainerView *group = [NSGlassEffectContainerView new];
            group.spacing = 0.0;
            NSView *content = [NSView new];
            [content addSubview:_stack];
            [NSLayoutConstraint activateConstraints:@[
                [_stack.topAnchor constraintEqualToAnchor:content.topAnchor],
                [_stack.leadingAnchor constraintEqualToAnchor:content.leadingAnchor],
                [_stack.trailingAnchor constraintEqualToAnchor:content.trailingAnchor],
                [_stack.bottomAnchor constraintEqualToAnchor:content.bottomAnchor],
            ]];
            group.contentView = content;
            content.translatesAutoresizingMaskIntoConstraints = NO;
            [NSLayoutConstraint activateConstraints:@[
                [content.topAnchor constraintEqualToAnchor:group.topAnchor],
                [content.bottomAnchor constraintEqualToAnchor:group.bottomAnchor],
                [content.leadingAnchor constraintEqualToAnchor:group.leadingAnchor],
                [content.trailingAnchor constraintEqualToAnchor:group.trailingAnchor],
            ]];
            group.translatesAutoresizingMaskIntoConstraints = NO;
            holder = group;
        }
    }
    [background addSubview:holder];
    [NSLayoutConstraint activateConstraints:@[
        [holder.topAnchor constraintEqualToAnchor:background.topAnchor],
        [holder.leadingAnchor constraintEqualToAnchor:background.leadingAnchor],
        [holder.trailingAnchor constraintEqualToAnchor:background.trailingAnchor],
        [holder.bottomAnchor constraintEqualToAnchor:background.bottomAnchor],
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
    // Liquid Glass draws its own depth; a window shadow would trace its halo as a dark outline.
    _panel.hasShadow = !liveGlass;
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
    if ([note.userInfo[KFPreferencesKey] isEqualToString:@"panelStyle"]) {
        // A new look is a new panel: built afresh at the next open.
        KFTheme.style = [_context.preferences.panelStyle isEqualToString:@"glass"]
                            ? KFThemeStyleGlass
                            : KFThemeStyleGraphite;
        [self close];
        _panel = nil;
        return;
    }
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
    _bannerTile.hidden = text == nil;
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

/// Live glass: whether every visible glass view under `view` holds its content (the only part
/// drawn above the glass) and the content fills it. Counts the glass views in `count`.
static BOOL GlassHoldsContent(NSView *view, NSUInteger *count) {
    if (view.hidden) {
        return YES;
    }
    if (@available(macOS 26.0, *)) {
        if ([view isKindOfClass:NSGlassEffectView.class]) {
            ++*count;
            NSView *content = ((NSGlassEffectView *)view).contentView;
            if (content == nil) {
                return NO;
            }
            const NSRect r = [content convertRect:content.bounds toView:view];
            if (fabs(NSWidth(r) - NSWidth(view.bounds)) > 0.5 ||
                fabs(NSHeight(r) - NSHeight(view.bounds)) > 0.5) {
                return NO;
            }
        }
    }
    for (NSView *sub in view.subviews) {
        if (!GlassHoldsContent(sub, count)) {
            return NO;
        }
    }
    return YES;
}

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
    _bannerTile.hidden = YES;
    _bigEnter.on = YES;
}

- (BOOL)renderPreviewToDirectory:(NSURL *)directory {
    const KFThemeStyle saved = KFTheme.style;
    BOOL ok = YES;
    for (NSNumber *style in @[ @(KFThemeStyleGraphite), @(KFThemeStyleGlass) ]) {
        KFTheme.style = (KFThemeStyle)style.integerValue;
        _simulateGlass = KFTheme.glass;
        _panel = nil;
        [self build];
        [self refreshAll];
        if (_simulateGlass) { // The desktop behind the tiles, sharp, and blurred for the glass.
            const NSSize size = _panel.contentView.fittingSize;
            _backdropBlur = Backdrop(size, YES);
            _panel = nil;
            [self build]; // Tiles take the blurred backdrop when built.
            [self refreshAll];
            _background.backdrop = Backdrop(size, NO);
        }
        NSString *name = KFTheme.glass ? @"glass" : @"graphite";
        ok = [self renderTo:directory suffix:[@"-" stringByAppendingString:name]] && ok;
        [self fillDemo];
        ok = [self renderTo:directory suffix:[NSString stringWithFormat:@"-%@-in-use", name]] && ok;
    }
    // The live glass cannot be drawn offscreen, but its layout can be checked: every tile
    // must have a real size.
    KFTheme.style = KFThemeStyleGlass;
    _simulateGlass = NO;
    _backdropBlur = nil;
    _panel = nil;
    [self build];
    [self refreshAll];
    [_panel.contentView layoutSubtreeIfNeeded];
    const NSSize size = _panel.contentView.fittingSize;
    [_panel setFrame:NSMakeRect(-10000, -10000, size.width, size.height) display:NO];
    [_panel.contentView layoutSubtreeIfNeeded];
    NSMutableArray<NSString *> *rows = [NSMutableArray array];
    for (NSView *row in _stack.arrangedSubviews) {
        if (!row.hidden) {
            [rows
                addObject:[NSString stringWithFormat:@"%@ %.0fx%.0f", row.className,
                                                     row.frame.size.width, row.frame.size.height]];
            ok = ok && row.frame.size.height >= 20.0 && row.frame.size.width > 300.0;
        }
    }
    NSUInteger tiles = 0;
    const BOOL held = GlassHoldsContent(_stack, &tiles);
    ok = ok && held;
    fprintf(stderr, "KFRenderPanel: live glass %.0fx%.0f: %s; %lu glass tiles %s\n", size.width,
            size.height, [rows componentsJoinedByString:@", "].UTF8String, (unsigned long)tiles,
            held ? "hold their content" : "DO NOT hold their content");
    KFTheme.style = saved;
    _panel = nil; // Rebuilt, live, at the next open.
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
