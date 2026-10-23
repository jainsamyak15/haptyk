// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "Menu/KFPanelControls.h"

#import "Support/KFTheme.h"

NSTextField *KFThemedLabel(NSString *text, NSFont *font, NSColor *color) {
    NSTextField *label = [NSTextField labelWithString:text];
    label.font = font;
    label.textColor = color;
    label.lineBreakMode = NSLineBreakByTruncatingTail;
    return label;
}

NSTextField *KFSectionLabel(NSString *title) {
    NSTextField *label = [NSTextField labelWithAttributedString:[KFTheme sectionTitle:title]];
    label.accessibilityLabel = title;
    return label;
}

/// A control's font, never nil.
static NSFont *FontOf(NSControl *control) {
    return control.font ?: [NSFont systemFontOfSize:12.0 weight:NSFontWeightMedium];
}

/// Hover tracking shared by the buttons below.
static void TrackHover(NSView *view) {
    for (NSTrackingArea *area in view.trackingAreas.copy) {
        [view removeTrackingArea:area];
    }
    [view addTrackingArea:[[NSTrackingArea alloc]
                              initWithRect:NSZeroRect
                                   options:NSTrackingMouseEnteredAndExited |
                                           NSTrackingActiveAlways | NSTrackingInVisibleRect
                                     owner:view
                                  userInfo:nil]];
}

#pragma mark - Pill switch

@implementation KFPillSwitch

- (instancetype)initWithFrame:(NSRect)frame {
    if ((self = [super initWithFrame:frame])) {
        [self setButtonType:NSButtonTypePushOnPushOff];
        self.bordered = NO;
        self.title = @"";
        self.focusRingType = NSFocusRingTypeExterior;
        self.accessibilityRole = NSAccessibilityCheckBoxRole;
        self.accessibilitySubrole = NSAccessibilitySwitchSubrole;
    }
    return self;
}

- (NSSize)intrinsicContentSize {
    return NSMakeSize(40.0, 24.0);
}

- (BOOL)isOn {
    return self.state == NSControlStateValueOn;
}

- (void)setOn:(BOOL)on {
    self.state = on ? NSControlStateValueOn : NSControlStateValueOff;
    [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirty {
    (void)dirty;
    const NSRect track = NSInsetRect(self.bounds, 1.0, 1.0);
    const CGFloat r = track.size.height / 2.0;
    [(self.on ? KFTheme.accent : KFTheme.track) setFill];
    [[NSBezierPath bezierPathWithRoundedRect:track xRadius:r yRadius:r] fill];
    const CGFloat d = track.size.height - 4.0;
    const CGFloat x = self.on ? NSMaxX(track) - d - 2.0 : NSMinX(track) + 2.0;
    const NSRect knob = NSMakeRect(x, NSMinY(track) + 2.0, d, d);
    [(self.on ? KFTheme.accentInk : KFTheme.textSecondary) setFill];
    [[NSBezierPath bezierPathWithOvalInRect:knob] fill];
}

- (void)drawFocusRingMask {
    const NSRect track = NSInsetRect(self.bounds, 1.0, 1.0);
    const CGFloat r = track.size.height / 2.0;
    [[NSBezierPath bezierPathWithRoundedRect:track xRadius:r yRadius:r] fill];
}

- (NSRect)focusRingMaskBounds {
    return self.bounds;
}

- (id)accessibilityValue {
    return @(self.on ? 1 : 0);
}

@end

#pragma mark - Slider

@interface KFLevelSliderCell : NSSliderCell
@end

@implementation KFLevelSliderCell

- (void)drawBarInside:(NSRect)rect flipped:(BOOL)flipped {
    const CGFloat h = 4.0;
    const NSRect track = NSMakeRect(NSMinX(rect), NSMidY(rect) - h / 2.0, rect.size.width, h);
    [KFTheme.track setFill];
    [[NSBezierPath bezierPathWithRoundedRect:track xRadius:h / 2.0 yRadius:h / 2.0] fill];
    const NSRect knob = [self knobRectFlipped:flipped];
    NSRect filled = track;
    filled.size.width = MAX(h, NSMidX(knob) - NSMinX(track));
    [KFTheme.accent setFill];
    [[NSBezierPath bezierPathWithRoundedRect:filled xRadius:h / 2.0 yRadius:h / 2.0] fill];
}

- (void)drawKnob:(NSRect)knobRect {
    const CGFloat d = 14.0;
    const NSRect knob = NSMakeRect(NSMidX(knobRect) - d / 2.0, NSMidY(knobRect) - d / 2.0, d, d);
    [KFTheme.background setFill];
    [[NSBezierPath bezierPathWithOvalInRect:NSInsetRect(knob, -2.0, -2.0)] fill];
    [KFTheme.accent setFill];
    [[NSBezierPath bezierPathWithOvalInRect:knob] fill];
    [KFTheme.accentInk setFill];
    [[NSBezierPath bezierPathWithOvalInRect:NSInsetRect(knob, 4.5, 4.5)] fill];
}

@end

@implementation KFLevelSlider

+ (Class)cellClass {
    return KFLevelSliderCell.class;
}

- (instancetype)initWithFrame:(NSRect)frame {
    if ((self = [super initWithFrame:frame])) {
        self.cell = [KFLevelSliderCell new];
        self.minValue = 0.0;
        self.maxValue = 1.0;
        self.continuous = YES;
    }
    return self;
}

- (NSSize)intrinsicContentSize {
    return NSMakeSize(NSViewNoIntrinsicMetric, 20.0);
}

@end

#pragma mark - Chip

@implementation KFChipButton {
    BOOL _hovered;
}

+ (instancetype)chipWithTitle:(NSString *)title
                       target:(nullable id)target
                       action:(nullable SEL)action {
    KFChipButton *chip = [[self alloc] initWithFrame:NSZeroRect];
    chip.title = title;
    chip.target = target;
    chip.action = action;
    return chip;
}

- (instancetype)initWithFrame:(NSRect)frame {
    if ((self = [super initWithFrame:frame])) {
        [self setButtonType:NSButtonTypePushOnPushOff];
        self.bordered = NO;
        self.font = [NSFont systemFontOfSize:11.5 weight:NSFontWeightMedium];
        self.focusRingType = NSFocusRingTypeExterior;
    }
    return self;
}

- (BOOL)isOn {
    return self.state == NSControlStateValueOn;
}

- (void)setOn:(BOOL)on {
    self.state = on ? NSControlStateValueOn : NSControlStateValueOff;
    [self setNeedsDisplay:YES];
}

- (void)setState:(NSControlStateValue)state {
    [super setState:state];
    [self setNeedsDisplay:YES];
}

- (NSSize)intrinsicContentSize {
    const NSSize text = [self.title sizeWithAttributes:@{NSFontAttributeName : FontOf(self)}];
    return NSMakeSize(ceil(text.width) + 20.0, 24.0);
}

- (void)updateTrackingAreas {
    [super updateTrackingAreas];
    TrackHover(self);
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

- (void)drawRect:(NSRect)dirty {
    (void)dirty;
    const NSRect pill = NSInsetRect(self.bounds, 0.5, 0.5);
    const CGFloat r = pill.size.height / 2.0;
    NSBezierPath *shape = [NSBezierPath bezierPathWithRoundedRect:pill xRadius:r yRadius:r];
    if (self.on) {
        [KFTheme.accent setFill];
        [shape fill];
    } else {
        [(_hovered ? KFTheme.surfaceHover : KFTheme.surface) setFill];
        [shape fill];
        [KFTheme.stroke setStroke];
        shape.lineWidth = 1.0;
        [shape stroke];
    }
    NSDictionary *attrs = @{
        NSFontAttributeName : FontOf(self),
        NSForegroundColorAttributeName : self.on
            ? KFTheme.accentInk
            : (_hovered ? KFTheme.textPrimary : KFTheme.textSecondary),
    };
    const NSSize size = [self.title sizeWithAttributes:attrs];
    [self.title drawAtPoint:NSMakePoint(NSMidX(self.bounds) - size.width / 2.0,
                                        NSMidY(self.bounds) - size.height / 2.0)
             withAttributes:attrs];
}

- (void)drawFocusRingMask {
    const NSRect pill = NSInsetRect(self.bounds, 0.5, 0.5);
    [[NSBezierPath bezierPathWithRoundedRect:pill
                                     xRadius:pill.size.height / 2.0
                                     yRadius:pill.size.height / 2.0] fill];
}

- (NSRect)focusRingMaskBounds {
    return self.bounds;
}

@end

#pragma mark - Link

@implementation KFLinkButton {
    BOOL _hovered;
}

+ (instancetype)linkWithTitle:(NSString *)title
                       target:(nullable id)target
                       action:(nullable SEL)action {
    KFLinkButton *link = [[self alloc] initWithFrame:NSZeroRect];
    link.title = title;
    link.target = target;
    link.action = action;
    return link;
}

- (instancetype)initWithFrame:(NSRect)frame {
    if ((self = [super initWithFrame:frame])) {
        self.bordered = NO;
        self.font = [NSFont systemFontOfSize:12.0 weight:NSFontWeightMedium];
        self.focusRingType = NSFocusRingTypeExterior;
    }
    return self;
}

- (NSSize)intrinsicContentSize {
    const NSSize text = [self.title sizeWithAttributes:@{NSFontAttributeName : FontOf(self)}];
    return NSMakeSize(ceil(text.width) + 4.0, 22.0);
}

- (void)updateTrackingAreas {
    [super updateTrackingAreas];
    TrackHover(self);
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

- (void)drawRect:(NSRect)dirty {
    (void)dirty;
    NSColor *color = self.prominent ? KFTheme.accent
                                    : (_hovered || self.isHighlighted ? KFTheme.textPrimary
                                                                      : KFTheme.textSecondary);
    NSDictionary *attrs =
        @{NSFontAttributeName : FontOf(self), NSForegroundColorAttributeName : color};
    const NSSize size = [self.title sizeWithAttributes:attrs];
    [self.title drawAtPoint:NSMakePoint(2.0, NSMidY(self.bounds) - size.height / 2.0)
             withAttributes:attrs];
}

@end

#pragma mark - Card and tiles

@implementation KFCardView

- (instancetype)initWithFrame:(NSRect)frame {
    if ((self = [super initWithFrame:frame])) {
        _cornerRadius = 12.0;
    }
    return self;
}

- (void)setGlassBacked:(BOOL)glassBacked {
    _glassBacked = glassBacked;
    [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirty {
    (void)dirty;
    if (self.glassBacked) {
        return; // The glass tile underneath is the surface.
    }
    const CGFloat radius = [KFTheme radius:self.cornerRadius];
    const NSRect card = NSInsetRect(self.bounds, 0.5, 0.5);
    NSBezierPath *shape = [NSBezierPath bezierPathWithRoundedRect:card
                                                          xRadius:radius
                                                          yRadius:radius];
    [(self.fillColor ?: KFTheme.surface) setFill];
    [shape fill];
    [KFTheme.stroke setStroke];
    shape.lineWidth = 1.0;
    [shape stroke];
}

@end

@implementation KFStatTile {
    NSTextField *_label;
    NSTextField *_value;
}

- (instancetype)initWithLabel:(NSString *)label {
    if ((self = [super initWithFrame:NSZeroRect])) {
        self.cornerRadius = 10.0;
        _label = KFThemedLabel(label, [NSFont systemFontOfSize:10.5 weight:NSFontWeightMedium],
                               KFTheme.textTertiary);
        _value = KFThemedLabel(@"–", [KFTheme numberFontOfSize:19.0 weight:NSFontWeightSemibold],
                               KFTheme.textPrimary);
        NSStackView *stack = [NSStackView stackViewWithViews:@[ _label, _value ]];
        stack.orientation = NSUserInterfaceLayoutOrientationVertical;
        stack.alignment = NSLayoutAttributeLeading;
        stack.spacing = 2.0;
        stack.edgeInsets = NSEdgeInsetsMake(9, 11, 9, 11);
        stack.translatesAutoresizingMaskIntoConstraints = NO;
        [self addSubview:stack];
        [NSLayoutConstraint activateConstraints:@[
            [stack.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
            [stack.trailingAnchor constraintLessThanOrEqualToAnchor:self.trailingAnchor],
            [stack.topAnchor constraintEqualToAnchor:self.topAnchor],
            [stack.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
        ]];
        self.accessibilityElement = YES;
        self.accessibilityRole = NSAccessibilityGroupRole;
    }
    return self;
}

- (NSString *)value {
    return _value.stringValue;
}

- (void)setValue:(NSString *)value {
    _value.stringValue = value;
    self.accessibilityLabel = [NSString stringWithFormat:@"%@: %@", _label.stringValue, value];
}

- (NSString *)label {
    return _label.stringValue;
}

- (void)setLabel:(NSString *)label {
    _label.stringValue = label;
    self.accessibilityLabel = [NSString stringWithFormat:@"%@: %@", label, _value.stringValue];
}

@end

#pragma mark - Type tag

@implementation KFTypeTag

- (NSDictionary *)attributes {
    return @{
        NSFontAttributeName : [NSFont systemFontOfSize:11.0 weight:NSFontWeightMedium],
        NSForegroundColorAttributeName : KFTheme.textTertiary,
    };
}

- (NSString *)text {
    return self.switchType.length > 0 ? self.switchType.capitalizedString : @"";
}

- (void)setSwitchType:(NSString *)switchType {
    _switchType = [switchType copy];
    self.accessibilityLabel = [self text];
    [self invalidateIntrinsicContentSize];
    [self setNeedsDisplay:YES];
}

- (NSSize)intrinsicContentSize {
    const NSSize text = [[self text] sizeWithAttributes:[self attributes]];
    return NSMakeSize(ceil(text.width) + 12.0, 16.0);
}

- (void)drawRect:(NSRect)dirty {
    (void)dirty;
    const CGFloat d = 6.0;
    [[KFTheme colorForSwitchType:self.switchType ?: @""] setFill];
    [[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(0, NSMidY(self.bounds) - d / 2.0, d, d)]
        fill];
    NSDictionary *attrs = [self attributes];
    const NSSize size = [[self text] sizeWithAttributes:attrs];
    [[self text] drawAtPoint:NSMakePoint(12.0, NSMidY(self.bounds) - size.height / 2.0)
              withAttributes:attrs];
}

@end
