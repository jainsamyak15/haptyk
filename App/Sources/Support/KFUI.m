// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "Support/KFUI.h"

static const CGFloat kLabelColumn = 170.0;
static const CGFloat kMargin = 20.0;

NSImage *KFSymbol(NSString *name, NSString *_Nullable description) {
    return [NSImage imageWithSystemSymbolName:name accessibilityDescription:description]
               ?: [[NSImage alloc] initWithSize:NSMakeSize(16, 16)];
}

NSTextField *KFLabel(NSString *text) {
    NSTextField *label = [NSTextField wrappingLabelWithString:text];
    label.selectable = NO;
    return label;
}

NSTextField *KFSecondaryLabel(NSString *text) {
    NSTextField *label = KFLabel(text);
    label.font = [NSFont systemFontOfSize:NSFont.smallSystemFontSize];
    label.textColor = NSColor.secondaryLabelColor;
    return label;
}

NSTextField *KFHeading(NSString *text) {
    NSTextField *label = [NSTextField labelWithString:text];
    label.font = [NSFont boldSystemFontOfSize:NSFont.systemFontSize + 1.0];
    label.accessibilityRole = NSAccessibilityStaticTextRole;
    return label;
}

NSButton *KFCheckbox(NSString *title, id _Nullable target, SEL _Nullable action) {
    return [NSButton checkboxWithTitle:title target:target action:action];
}

NSButton *KFPushButton(NSString *title, id _Nullable target, SEL _Nullable action) {
    return [NSButton buttonWithTitle:title target:target action:action];
}

NSSlider *KFSlider(double min, double max, double value, id _Nullable target, SEL _Nullable action,
                   NSString *accessibilityLabel) {
    NSSlider *slider = [NSSlider sliderWithValue:value
                                        minValue:min
                                        maxValue:max
                                          target:target
                                          action:action];
    slider.continuous = YES;
    slider.accessibilityLabel = accessibilityLabel;
    [slider.widthAnchor constraintGreaterThanOrEqualToConstant:200.0].active = YES;
    return slider;
}

static NSStackView *Stack(NSArray<NSView *> *views, CGFloat spacing,
                          NSUserInterfaceLayoutOrientation orientation) {
    NSStackView *stack = [NSStackView stackViewWithViews:views];
    stack.orientation = orientation;
    stack.spacing = spacing;
    stack.alignment = orientation == NSUserInterfaceLayoutOrientationVertical
                          ? NSLayoutAttributeLeading
                          : NSLayoutAttributeCenterY;
    return stack;
}

NSStackView *KFVStack(NSArray<NSView *> *views, CGFloat spacing) {
    return Stack(views, spacing, NSUserInterfaceLayoutOrientationVertical);
}

NSStackView *KFHStack(NSArray<NSView *> *views, CGFloat spacing) {
    return Stack(views, spacing, NSUserInterfaceLayoutOrientationHorizontal);
}

NSView *KFFormRow(NSString *label, NSView *control) {
    NSTextField *title = [NSTextField labelWithString:label];
    title.alignment = NSTextAlignmentRight;
    [title.widthAnchor constraintEqualToConstant:kLabelColumn].active = YES;
    if (control.accessibilityLabel.length == 0) {
        control.accessibilityLabel = label;
    }
    NSStackView *row = KFHStack(@[ title, control ], 10.0);
    row.alignment = NSLayoutAttributeFirstBaseline;
    return row;
}

NSView *KFPane(NSArray<NSView *> *rows, CGFloat width) {
    NSStackView *stack = KFVStack(rows, 14.0);
    stack.edgeInsets = NSEdgeInsetsMake(kMargin, kMargin, kMargin, kMargin);
    stack.translatesAutoresizingMaskIntoConstraints = NO;
    NSView *container = [NSView new];
    [container addSubview:stack];
    [NSLayoutConstraint activateConstraints:@[
        [stack.topAnchor constraintEqualToAnchor:container.topAnchor],
        [stack.leadingAnchor constraintEqualToAnchor:container.leadingAnchor],
        [stack.trailingAnchor constraintEqualToAnchor:container.trailingAnchor],
        [stack.bottomAnchor constraintEqualToAnchor:container.bottomAnchor],
        [container.widthAnchor constraintEqualToConstant:width],
    ]];
    return container;
}
