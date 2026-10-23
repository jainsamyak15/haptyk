// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "Support/KFTheme.h"

static NSColor *RGB(uint32_t hex) {
    return [NSColor colorWithSRGBRed:(CGFloat)((hex >> 16) & 0xff) / 255.0
                               green:(CGFloat)((hex >> 8) & 0xff) / 255.0
                                blue:(CGFloat)(hex & 0xff) / 255.0
                               alpha:1.0];
}

static BOOL IsDark(NSAppearance *appearance) {
    NSAppearanceName match = [appearance bestMatchFromAppearancesWithNames:@[
        NSAppearanceNameAqua, NSAppearanceNameDarkAqua,
        NSAppearanceNameAccessibilityHighContrastAqua,
        NSAppearanceNameAccessibilityHighContrastDarkAqua
    ]];
    return [match isEqualToString:NSAppearanceNameDarkAqua] ||
           [match isEqualToString:NSAppearanceNameAccessibilityHighContrastDarkAqua];
}

static BOOL IsHighContrast(NSAppearance *appearance) {
    NSAppearanceName match = [appearance bestMatchFromAppearancesWithNames:@[
        NSAppearanceNameAqua, NSAppearanceNameDarkAqua,
        NSAppearanceNameAccessibilityHighContrastAqua,
        NSAppearanceNameAccessibilityHighContrastDarkAqua
    ]];
    return [match hasPrefix:@"NSAppearanceNameAccessibility"];
}

/// light, dark, and the Increase Contrast variants (0 = same as the normal one).
static NSColor *Dynamic(uint32_t light, uint32_t dark, uint32_t lightHC, uint32_t darkHC) {
    return [NSColor colorWithName:nil
                  dynamicProvider:^NSColor *(NSAppearance *appearance) {
                      const BOOL dark_ = IsDark(appearance);
                      if (IsHighContrast(appearance)) {
                          const uint32_t hc = dark_ ? darkHC : lightHC;
                          if (hc != 0) {
                              return RGB(hc);
                          }
                      }
                      return RGB(dark_ ? dark : light);
                  }];
}

@implementation KFTheme

+ (NSColor *)background {
    return Dynamic(0xF5F3EF, 0x121211, 0, 0);
}

+ (NSColor *)surface {
    return Dynamic(0xFFFFFF, 0x1C1B19, 0, 0);
}

+ (NSColor *)surfaceHover {
    return Dynamic(0xEFEBE4, 0x272523, 0, 0);
}

+ (NSColor *)stroke {
    return Dynamic(0xE2DDD4, 0x2E2C29, 0x9C9589, 0x5C5852);
}

+ (NSColor *)track {
    return Dynamic(0xE3DED5, 0x302E2B, 0xB3ACA0, 0x55514B);
}

+ (NSColor *)textPrimary {
    return Dynamic(0x1B1A18, 0xF2EFE9, 0x000000, 0xFFFFFF);
}

+ (NSColor *)textSecondary {
    return Dynamic(0x57534B, 0xB7B2A8, 0x2F2C27, 0xDCD7CD);
}

+ (NSColor *)textTertiary {
    return Dynamic(0x75706A, 0x8C877E, 0x4A463F, 0xB7B2A8);
}

+ (NSColor *)accent {
    return Dynamic(0xEC560C, 0xFF6B1F, 0xC24300, 0xFF7A33);
}

+ (NSColor *)accentInk {
    return Dynamic(0x1B0F05, 0x1B0F05, 0x000000, 0x000000);
}

+ (NSColor *)accentSoft {
    return [NSColor colorWithName:nil
                  dynamicProvider:^NSColor *(NSAppearance *appearance) {
                      return [RGB(IsDark(appearance) ? 0xFF6B1F : 0xEC560C)
                          colorWithAlphaComponent:IsDark(appearance) ? 0.16 : 0.12];
                  }];
}

+ (NSColor *)colorForSwitchType:(NSString *)switchType {
    if ([switchType isEqualToString:@"linear"]) {
        return Dynamic(0x3D875A, 0x8CC19E, 0, 0);
    }
    if ([switchType isEqualToString:@"tactile"]) {
        return Dynamic(0xA87719, 0xE5B65E, 0, 0);
    }
    if ([switchType isEqualToString:@"clicky"]) {
        return Dynamic(0x2E6BAB, 0x74AEE6, 0, 0);
    }
    if ([switchType isEqualToString:@"topre"]) {
        return Dynamic(0x586575, 0xA2AEBB, 0, 0);
    }
    if ([switchType isEqualToString:@"buckling"]) {
        return Dynamic(0xA2502C, 0xD88D67, 0, 0);
    }
    return Dynamic(0x6B665C, 0xB0AA9F, 0, 0);
}

+ (NSFont *)sectionFont {
    return [NSFont systemFontOfSize:10.5 weight:NSFontWeightSemibold];
}

+ (NSFont *)monoFontOfSize:(CGFloat)size weight:(NSFontWeight)weight {
    return [NSFont monospacedSystemFontOfSize:size weight:weight];
}

+ (NSFont *)numberFontOfSize:(CGFloat)size weight:(NSFontWeight)weight {
    return [NSFont monospacedDigitSystemFontOfSize:size weight:weight];
}

+ (NSAttributedString *)sectionTitle:(NSString *)title {
    return [[NSAttributedString alloc] initWithString:title.uppercaseString
                                           attributes:@{
                                               NSFontAttributeName : self.sectionFont,
                                               NSForegroundColorAttributeName : self.textTertiary,
                                               NSKernAttributeName : @0.9,
                                           }];
}

+ (BOOL)reduceMotion {
    return NSWorkspace.sharedWorkspace.accessibilityDisplayShouldReduceMotion;
}

@end
