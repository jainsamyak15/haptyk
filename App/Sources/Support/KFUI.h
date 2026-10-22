// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

/// Small builders for programmatic AppKit layout (no nibs). Every control that has no visible
/// text gets an accessibility label from its caller.

/// An SF Symbol, or an empty image if this macOS lacks it.
NSImage *KFSymbol(NSString *name, NSString *_Nullable description);
NSTextField *KFLabel(NSString *text);
NSTextField *KFSecondaryLabel(NSString *text);
NSTextField *KFHeading(NSString *text);
NSButton *KFCheckbox(NSString *title, id _Nullable target, SEL _Nullable action);
NSButton *KFPushButton(NSString *title, id _Nullable target, SEL _Nullable action);
NSSlider *KFSlider(double min, double max, double value, id _Nullable target, SEL _Nullable action,
                   NSString *accessibilityLabel);
NSStackView *KFVStack(NSArray<NSView *> *views, CGFloat spacing);
NSStackView *KFHStack(NSArray<NSView *> *views, CGFloat spacing);
/// A row with a fixed-width trailing-aligned label column, like System Settings.
NSView *KFFormRow(NSString *label, NSView *control);
/// Wraps a pane's content with standard margins and a minimum width.
NSView *KFPane(NSArray<NSView *> *rows, CGFloat width);

NS_ASSUME_NONNULL_END
