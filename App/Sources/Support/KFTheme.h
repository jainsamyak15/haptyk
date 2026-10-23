// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

/// The status panel's two looks (ADR-0023, ADR-0024).
typedef NS_ENUM(NSInteger, KFThemeStyle) {
    /// "Graphite & Signal": warm graphite neutrals with one signal-orange accent, the colour of
    /// an enthusiast keyboard's Escape keycap. Flat, no gradients.
    KFThemeStyleGraphite = 0,
    /// "Glass": translucent tiles over the desktop, like macOS Control Center; Liquid Glass on
    /// macOS 26 and later, the system blur before. Active controls are white (near-black in
    /// light appearance); orange stays only in the wordmark.
    KFThemeStyleGlass,
};

/// Design tokens. Every colour is dynamic: it resolves for the current style, light or dark
/// appearance, and Increase Contrast, wherever it is drawn, so a style change needs only a redraw.
@interface KFTheme : NSObject

@property (class, nonatomic) KFThemeStyle style;
@property (class, nonatomic, readonly) BOOL glass;

@property (class, nonatomic, readonly) NSColor *background; ///< The panel itself (clear in Glass).
@property (class, nonatomic, readonly) NSColor *surface;    ///< Cards, rows, tiles.
@property (class, nonatomic, readonly) NSColor *surfaceHover; ///< A hovered or pressed surface.
@property (class, nonatomic, readonly) NSColor *stroke;       ///< Hairlines and outlines.
@property (class, nonatomic, readonly) NSColor *track;        ///< Unfilled slider and meter parts.
@property (class, nonatomic, readonly) NSColor *textPrimary;
@property (class, nonatomic, readonly) NSColor *textSecondary;
@property (class, nonatomic, readonly) NSColor *textTertiary; ///< Section labels, captions.
@property (class, nonatomic, readonly) NSColor *accent;       ///< Signal orange; white in Glass.
@property (class, nonatomic, readonly) NSColor *accentInk;    ///< Text and icons on the accent.
@property (class, nonatomic, readonly) NSColor *accentSoft;   ///< A quiet accent wash.
@property (class, nonatomic, readonly) NSColor *brand;        ///< Signal orange in every style.

/// Muted colour of a switch type's tag: linear sage, tactile amber, clicky azure, topre slate,
/// buckling terracotta, other stone.
+ (NSColor *)colorForSwitchType:(NSString *)switchType;

/// A corner radius for the current style: Glass rounds more, as Control Center does.
+ (CGFloat)radius:(CGFloat)graphite;

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
