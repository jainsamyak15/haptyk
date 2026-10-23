// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

/// The status panel's controls, drawn in KFTheme colours. Each is a native control underneath
/// (NSButton, NSSlider), so keyboard use, VoiceOver and focus rings work as for any control.

/// An on/off switch: a capsule track, accent when on, with a sliding knob.
@interface KFPillSwitch : NSButton
@property (nonatomic, getter=isOn) BOOL on;
@end

/// A thin slider with an accent fill and a round knob.
@interface KFLevelSlider : NSSlider
@end

/// A capsule chip: a filter (one of a group) or a toggle. On: accent fill. Off: outline.
@interface KFChipButton : NSButton
@property (nonatomic, getter=isOn) BOOL on;
+ (instancetype)chipWithTitle:(NSString *)title
                       target:(nullable id)target
                       action:(nullable SEL)action;
@end

/// A text-only button: secondary text, primary on hover; accent when prominent.
@interface KFLinkButton : NSButton
@property (nonatomic) BOOL prominent;
+ (instancetype)linkWithTitle:(NSString *)title
                       target:(nullable id)target
                       action:(nullable SEL)action;
@end

/// A rounded surface with a hairline: cards and tiles.
@interface KFCardView : NSView
@property (nonatomic) CGFloat cornerRadius; ///< Graphite radius; Glass rounds more (KFTheme).
@property (nonatomic, nullable) NSColor *fillColor; ///< Default KFTheme.surface.
/// Set when the card sits on a glass tile, which then provides its surface: no fill or line.
@property (nonatomic) BOOL glassBacked;
@end

/// A small stat: label above, value below.
@interface KFStatTile : KFCardView
- (instancetype)initWithLabel:(NSString *)label;
@property (nonatomic, copy) NSString *value;
@property (nonatomic, copy) NSString *label;
@end

/// A switch type's coloured dot and name, for pack rows.
@interface KFTypeTag : NSView
@property (nonatomic, copy) NSString *switchType;
@end

/// A section title label.
NSTextField *KFSectionLabel(NSString *title);
/// A plain label in a theme colour.
NSTextField *KFThemedLabel(NSString *text, NSFont *font, NSColor *color);

NS_ASSUME_NONNULL_END
