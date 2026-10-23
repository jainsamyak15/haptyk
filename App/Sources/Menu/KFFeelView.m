// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "Menu/KFFeelView.h"

#include <math.h>

#import "Labs/KFForceMeterController.h"
#import "Support/KFTheme.h"

const NSUInteger KFFeelActivitySeconds = 60;

static NSArray<NSString *> *TierNames(void) {
    return @[ @"Soft", @"Medium", @"Hard", @"Slam" ];
}

@implementation KFFeelView {
    float _velocity;
    NSString *_status;
    uint32_t _activity[60];
    NSUInteger _tapsPerMinute;
}

- (instancetype)initWithFrame:(NSRect)frame {
    if ((self = [super initWithFrame:frame])) {
        _velocity = -1.0f;
        _status = @"";
        self.accessibilityElement = YES;
        self.accessibilityRole = NSAccessibilityGroupRole;
        [self.heightAnchor constraintEqualToConstant:118.0].active = YES;
        [self updateAccessibility];
    }
    return self;
}

- (BOOL)isFlipped {
    return YES;
}

- (void)setVelocity:(float)velocity status:(NSString *)status {
    _velocity = velocity;
    _status = [status copy];
    [self updateAccessibility];
    [self setNeedsDisplay:YES];
}

- (void)setActivity:(const uint32_t *)counts tapsPerMinute:(NSUInteger)tapsPerMinute {
    memcpy(_activity, counts, sizeof _activity);
    _tapsPerMinute = tapsPerMinute;
    [self updateAccessibility];
    [self setNeedsDisplay:YES];
}

- (NSInteger)tierIndex {
    if (!(_velocity >= 0.0f)) {
        return -1;
    }
    return (NSInteger)
        [TierNames() indexOfObject:[KFForceMeterController tierNameForVelocity:_velocity]];
}

- (void)updateAccessibility {
    const NSInteger tier = [self tierIndex];
    self.accessibilityLabel = [NSString
        stringWithFormat:@"Feel. Last keystroke: %@. %@. %lu taps in the last minute.",
                         tier >= 0 ? TierNames()[(NSUInteger)tier] : @"force not measured", _status,
                         (unsigned long)_tapsPerMinute];
}

static void DrawText(NSString *text, NSPoint at, NSFont *font, NSColor *color) {
    [text drawAtPoint:at
        withAttributes:@{NSFontAttributeName : font, NSForegroundColorAttributeName : color}];
}

static CGFloat Width(NSString *text, NSFont *font) {
    return [text sizeWithAttributes:@{NSFontAttributeName : font}].width;
}

- (void)drawRect:(NSRect)dirty {
    [super drawRect:dirty];
    const CGFloat pad = 14.0;
    const NSRect b = self.bounds;

    // Top: the label and the force status.
    NSAttributedString *title = [KFTheme sectionTitle:@"Last keystroke"];
    [title drawAtPoint:NSMakePoint(pad, pad - 2.0)];
    NSFont *small = [NSFont systemFontOfSize:11.0 weight:NSFontWeightMedium];
    DrawText(_status, NSMakePoint(NSMaxX(b) - pad - Width(_status, small), pad - 3.0), small,
             KFTheme.textTertiary);

    // Middle: the tier as a keycap legend, then four rising bars.
    const NSInteger tier = [self tierIndex];
    NSFont *legend = [KFTheme monoFontOfSize:22.0 weight:NSFontWeightSemibold];
    NSString *word = tier >= 0 ? TierNames()[(NSUInteger)tier].uppercaseString : @"—";
    DrawText(word, NSMakePoint(pad, pad + 14.0), legend,
             tier >= 0 ? KFTheme.accent : KFTheme.textTertiary);
    NSFont *num = [KFTheme numberFontOfSize:12.0 weight:NSFontWeightMedium];
    NSString *aside =
        tier >= 0 ? [NSString stringWithFormat:@"%.0f%%", (double)fminf(_velocity, 1.0f) * 100.0]
                  : @"plays at one level";
    DrawText(aside, NSMakePoint(pad + Width(word, legend) + 8.0, pad + 24.0),
             tier >= 0 ? num : [NSFont systemFontOfSize:12.0 weight:NSFontWeightRegular],
             KFTheme.textSecondary);
    const CGFloat barW = 7.0;
    const CGFloat gap = 4.0;
    const CGFloat base = pad + 42.0;
    for (NSInteger i = 0; i < 4; ++i) {
        const CGFloat h = 8.0 + 6.0 * (CGFloat)i;
        const NSRect bar =
            NSMakeRect(NSMaxX(b) - pad - (4 - i) * (barW + gap) + gap, base - h, barW, h);
        [(i <= tier ? KFTheme.accent : KFTheme.track) setFill];
        [[NSBezierPath bezierPathWithRoundedRect:bar xRadius:2.0 yRadius:2.0] fill];
    }

    // Bottom: one bar per second of the last minute, the last five in the accent.
    const CGFloat chartTop = pad + 54.0;
    const CGFloat chartH = 26.0;
    const NSUInteger n = KFFeelActivitySeconds;
    uint32_t peak = 6u; // Below a few keys a second, bars stay low rather than jumping to full.
    for (NSUInteger i = 0; i < n; ++i) {
        peak = MAX(peak, _activity[i]);
    }
    const CGFloat width = b.size.width - 2.0 * pad;
    const CGFloat slot = width / (CGFloat)n;
    for (NSUInteger i = 0; i < n; ++i) {
        const CGFloat h = MAX(2.0, chartH * (CGFloat)_activity[i] / (CGFloat)peak);
        const NSRect bar =
            NSMakeRect(pad + slot * (CGFloat)i, chartTop + chartH - h, MAX(1.0, slot - 1.5), h);
        NSColor *color = _activity[i] == 0 ? KFTheme.track
                         : i >= n - 5      ? KFTheme.accent
                                           : KFTheme.textTertiary;
        [color setFill];
        [[NSBezierPath bezierPathWithRoundedRect:bar xRadius:1.0 yRadius:1.0] fill];
    }
    NSString *left = @"Typing, last minute";
    NSString *right = [NSString stringWithFormat:@"%lu taps/min", (unsigned long)_tapsPerMinute];
    NSFont *caption = [NSFont systemFontOfSize:10.5 weight:NSFontWeightRegular];
    DrawText(left, NSMakePoint(pad, chartTop + chartH + 5.0), caption, KFTheme.textTertiary);
    NSFont *captionNum = [KFTheme numberFontOfSize:10.5 weight:NSFontWeightMedium];
    DrawText(right,
             NSMakePoint(NSMaxX(b) - pad - Width(right, captionNum), chartTop + chartH + 5.0),
             captionNum, KFTheme.textSecondary);
}

@end
