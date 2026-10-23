// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

/// "Graphite & Signal": warm graphite neutrals with one signal-orange accent, the colour of an
/// enthusiast keyboard's Escape keycap. Flat, no gradients. Every colour is dynamic: it resolves
/// for light or dark appearance, and for Increase Contrast, wherever it is drawn.
@interface KFTheme : NSObject

@property (class, nonatomic, readonly) NSColor *background;   ///< The panel itself.
@property (class, nonatomic, readonly) NSColor *surface;      ///< Cards, rows, tiles.
@property (class, nonatomic, readonly) NSColor *surfaceHover; ///< A hovered or pressed surface.
@property (class, nonatomic, readonly) NSColor *stroke;       ///< Hairlines and outlines.
@property (class, nonatomic, readonly) NSColor *track;        ///< Unfilled slider and meter parts.
@property (class, nonatomic, readonly) NSColor *textPrimary;
@property (class, nonatomic, readonly) NSColor *textSecondary;
@property (class, nonatomic, readonly) NSColor *textTertiary; ///< Section labels, captions.
@property (class, nonatomic, readonly) NSColor *accent;       ///< Signal orange.
@property (class, nonatomic, readonly) NSColor *accentInk;    ///< Text and icons on the accent.
@property (class, nonatomic, readonly) NSColor *accentSoft;   ///< A quiet accent wash.

/// Muted colour of a switch type's tag: linear sage, tactile amber, clicky azure, topre slate,
/// buckling terracotta, other stone.
+ (NSColor *)colorForSwitchType:(NSString *)switchType;

/// Small uppercase section labels.
+ (NSFont *)sectionFont;
/// SF Mono, for keycap-legend style values.
+ (NSFont *)monoFontOfSize:(CGFloat)size weight:(NSFontWeight)weight;
/// Tabular figures for live numbers.
+ (NSFont *)numberFontOfSize:(CGFloat)size weight:(NSFontWeight)weight;

/// Section label text: uppercase, tracked, in textTertiary.
+ (NSAttributedString *)sectionTitle:(NSString *)title;

/// Whether motion should be skipped (Reduce Motion).
+ (BOOL)reduceMotion;

@end

NS_ASSUME_NONNULL_END
