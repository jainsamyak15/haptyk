// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "Menu/KFStatusIcon.h"

@implementation KFStatusIcon

/// A keycap seen from above: an outer rounded square with the dished top inside.
static void DrawKeycap(NSRect rect, CGFloat alpha) {
    [[NSColor colorWithWhite:0.0 alpha:alpha] set];
    NSBezierPath *outer = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(rect, 1.5, 1.5)
                                                          xRadius:3.5
                                                          yRadius:3.5];
    outer.lineWidth = 1.5;
    [outer stroke];
    NSRect top = NSInsetRect(rect, 5.0, 5.0);
    top.origin.y += 1.0;
    NSBezierPath *dish = [NSBezierPath bezierPathWithRoundedRect:top xRadius:2.0 yRadius:2.0];
    [dish fill];
}

+ (NSImage *)imageForState:(KFStatusIconState)state {
    const NSSize size = NSMakeSize(18.0, 18.0);
    NSImage *image = [NSImage imageWithSize:size
                                    flipped:NO
                             drawingHandler:^BOOL(NSRect rect) {
                                 DrawKeycap(rect, state == KFStatusIconOff ? 0.35 : 1.0);
                                 if (state == KFStatusIconAttention) {
                                     [[NSColor colorWithWhite:0.0 alpha:1.0] set];
                                     const NSRect dot = NSMakeRect(
                                         rect.size.width - 7.0, rect.size.height - 7.0, 6.0, 6.0);
                                     [[NSBezierPath bezierPathWithOvalInRect:dot] fill];
                                 }
                                 return YES;
                             }];
    image.template = YES;
    image.accessibilityDescription =
        state == KFStatusIconAttention ? @"Keyfeel, needs attention"
                                       : (state == KFStatusIconOff ? @"Keyfeel, off" : @"Keyfeel");
    return image;
}

@end
