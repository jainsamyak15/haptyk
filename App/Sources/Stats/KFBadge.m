// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "Stats/KFBadge.h"

#include <math.h>

#include <kf/kf_config.h>

static const NSUInteger kRequiredKeystrokes = 500;

@implementation KFBadge

+ (NSUInteger)requiredKeystrokes {
    return kRequiredKeystrokes;
}

+ (double)medianForceOfHistogram:(NSArray<NSNumber *> *)histogram {
    double total = 0.0;
    for (NSNumber *n in histogram) {
        total += n.doubleValue;
    }
    if (histogram.count != KF_STATS_FORCE_BUCKETS || !(total > 0.0)) {
        return NAN;
    }
    const double lo = log((double)KF_STATS_FORCE_MIN_G);
    const double step = (log((double)KF_STATS_FORCE_MAX_G) - lo) / (double)KF_STATS_FORCE_BUCKETS;
    double seen = 0.0;
    for (NSUInteger i = 0; i < histogram.count; ++i) {
        const double count = histogram[i].doubleValue;
        if (count > 0.0 && seen + count >= total / 2.0) {
            const double within = (total / 2.0 - seen) / count; /* 0..1 inside this bucket. */
            return exp(lo + step * ((double)i + within));
        }
        seen += count;
    }
    return NAN;
}

+ (double)percentileOfForce:(double)force table:(NSArray<NSNumber *> *)table {
    if (table.count < 2 || !isfinite(force)) {
        return NAN;
    }
    for (NSUInteger i = 1; i < table.count; ++i) {
        if (table[i].doubleValue < table[i - 1].doubleValue) {
            return NAN; /* Not ascending: refuse rather than mislead. */
        }
    }
    const double last = (double)(table.count - 1);
    if (force <= table.firstObject.doubleValue) {
        return 0.0;
    }
    if (force >= table.lastObject.doubleValue) {
        return 100.0;
    }
    for (NSUInteger i = 1; i < table.count; ++i) {
        const double a = table[i - 1].doubleValue;
        const double b = table[i].doubleValue;
        if (force <= b) {
            const double t = b > a ? (force - a) / (b - a) : 0.0;
            return ((double)(i - 1) + t) / last * 100.0;
        }
    }
    return 100.0;
}

+ (KFBadgeKind)badgeForPercentile:(double)percentile {
    if (percentile >= 90.0) {
        return KFBadgeThunder;
    }
    if (percentile >= 60.0) {
        return KFBadgeHammer;
    }
    if (percentile >= 25.0) {
        return KFBadgeSteady;
    }
    return KFBadgeFeather;
}

+ (NSString *)nameOfBadge:(KFBadgeKind)badge {
    switch (badge) {
    case KFBadgeFeather:
        return @"Feather";
    case KFBadgeSteady:
        return @"Steady";
    case KFBadgeHammer:
        return @"Hammer";
    case KFBadgeThunder:
        return @"Thunder";
    }
    return @"";
}

static NSString *SymbolForBadge(KFBadgeKind badge) {
    switch (badge) {
    case KFBadgeFeather:
        return @"leaf.fill";
    case KFBadgeSteady:
        return @"metronome.fill";
    case KFBadgeHammer:
        return @"hammer.fill";
    case KFBadgeThunder:
        return @"bolt.fill";
    }
    return @"keyboard";
}

+ (nullable NSData *)cardWithBadge:(KFBadgeKind)badge
                        percentile:(double)percentile
                          packName:(NSString *)packName {
    NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
                                                                    pixelsWide:1200
                                                                    pixelsHigh:630
                                                                 bitsPerSample:8
                                                               samplesPerPixel:4
                                                                      hasAlpha:YES
                                                                      isPlanar:NO
                                                                colorSpaceName:NSDeviceRGBColorSpace
                                                                   bytesPerRow:0
                                                                  bitsPerPixel:0];
    NSGraphicsContext *context =
        rep != nil ? [NSGraphicsContext graphicsContextWithBitmapImageRep:rep] : nil;
    if (context == nil) {
        return nil;
    }
    [NSGraphicsContext saveGraphicsState];
    NSGraphicsContext.currentContext = context;
    /* A fixed dark card: shared images look the same in every viewer. */
    [[NSColor colorWithSRGBRed:0.10 green:0.10 blue:0.10 alpha:1.0] setFill];
    NSRectFill(NSMakeRect(0, 0, 1200, 630));
    NSImage *symbol = [NSImage imageWithSystemSymbolName:SymbolForBadge(badge)
                                accessibilityDescription:nil];
    NSImageSymbolConfiguration *config =
        [NSImageSymbolConfiguration configurationWithPointSize:180 weight:NSFontWeightRegular];
    symbol = [symbol imageWithSymbolConfiguration:config];
    if (symbol != nil) {
        NSImage *tinted =
            [NSImage imageWithSize:symbol.size
                           flipped:NO
                    drawingHandler:^BOOL(NSRect rect) {
                        [symbol drawInRect:rect];
                        [[NSColor colorWithSRGBRed:0.22 green:0.53 blue:0.90 alpha:1.0] set];
                        NSRectFillUsingOperation(rect, NSCompositingOperationSourceAtop);
                        return YES;
                    }];
        [tinted drawInRect:NSMakeRect(110, 215, symbol.size.width, symbol.size.height)];
    }
    NSColor *white = [NSColor colorWithSRGBRed:0.94 green:0.94 blue:0.93 alpha:1.0];
    NSColor *grey = [NSColor colorWithSRGBRed:0.76 green:0.76 blue:0.72 alpha:1.0];
    NSDictionary *title = @{
        NSFontAttributeName : [NSFont systemFontOfSize:96 weight:NSFontWeightBold],
        NSForegroundColorAttributeName : white
    };
    NSDictionary *body = @{
        NSFontAttributeName : [NSFont systemFontOfSize:44 weight:NSFontWeightMedium],
        NSForegroundColorAttributeName : white
    };
    NSDictionary *small = @{
        NSFontAttributeName : [NSFont systemFontOfSize:32 weight:NSFontWeightRegular],
        NSForegroundColorAttributeName : grey
    };
    [[KFBadge nameOfBadge:badge] drawAtPoint:NSMakePoint(440, 340) withAttributes:title];
    [[NSString stringWithFormat:@"Types harder than %.0f%% of Keyfeel users", percentile]
           drawAtPoint:NSMakePoint(444, 270)
        withAttributes:body];
    [[NSString stringWithFormat:@"on %@", packName] drawAtPoint:NSMakePoint(444, 215)
                                                 withAttributes:small];
    [@"Keyfeel" drawAtPoint:NSMakePoint(110, 70) withAttributes:small];
    [NSGraphicsContext restoreGraphicsState];
    return [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
}

@end
