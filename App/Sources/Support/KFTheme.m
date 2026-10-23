// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "Support/KFTheme.h"

static KFThemeStyle g_style = KFThemeStyleGraphite;

/// 0xRRGGBBAA.
static NSColor *RGBA(uint32_t hex) {
    return [NSColor colorWithSRGBRed:(CGFloat)((hex >> 24) & 0xff) / 255.0
                               green:(CGFloat)((hex >> 16) & 0xff) / 255.0
                                blue:(CGFloat)((hex >> 8) & 0xff) / 255.0
                               alpha:(CGFloat)(hex & 0xff) / 255.0];
}

static NSAppearanceName Match(NSAppearance *appearance) {
    return [appearance bestMatchFromAppearancesWithNames:@[
        NSAppearanceNameAqua, NSAppearanceNameDarkAqua,
        NSAppearanceNameAccessibilityHighContrastAqua,
        NSAppearanceNameAccessibilityHighContrastDarkAqua
    ]];
}

static BOOL IsDark(NSAppearance *appearance) {
    NSAppearanceName match = Match(appearance);
    return [match isEqualToString:NSAppearanceNameDarkAqua] ||
           [match isEqualToString:NSAppearanceNameAccessibilityHighContrastDarkAqua];
}

static BOOL IsHighContrast(NSAppearance *appearance) {
    return [Match(appearance) hasPrefix:@"NSAppearanceNameAccessibility"];
}

/// One token: its Graphite and Glass values, light and dark, and Increase Contrast overrides
/// (0 = same as the normal value). Colours are 0xRRGGBBAA.
typedef struct Token {
    uint32_t light, dark, lightHC, darkHC;
    uint32_t glassLight, glassDark, glassLightHC, glassDarkHC;
} Token;

static NSColor *Dynamic(Token t) {
    return [NSColor colorWithName:nil
                  dynamicProvider:^NSColor *(NSAppearance *appearance) {
                      const BOOL dark = IsDark(appearance);
                      const BOOL hc = IsHighContrast(appearance);
                      const BOOL glass = g_style == KFThemeStyleGlass;
                      uint32_t value =
                          glass ? (dark ? t.glassDark : t.glassLight) : (dark ? t.dark : t.light);
                      const uint32_t contrast = glass ? (dark ? t.glassDarkHC : t.glassLightHC)
                                                      : (dark ? t.darkHC : t.lightHC);
                      if (hc && contrast != 0) {
                          value = contrast;
                      }
                      return RGBA(value);
                  }];
}

@implementation KFTheme

+ (KFThemeStyle)style {
    return g_style;
}

+ (void)setStyle:(KFThemeStyle)style {
    g_style = style;
}

+ (BOOL)glass {
    return g_style == KFThemeStyleGlass;
}

+ (NSColor *)background {
    return Dynamic((Token){0xF5F3EFFF, 0x121211FF, 0, 0, 0x00000000, 0x00000000, 0, 0});
}

+ (NSColor *)surface {
    return Dynamic(
        (Token){0xFFFFFFFF, 0x1C1B19FF, 0, 0, 0xFFFFFF59, 0xFFFFFF14, 0xFFFFFFB3, 0x00000099});
}

+ (NSColor *)surfaceHover {
    return Dynamic((Token){0xEFEBE4FF, 0x272523FF, 0, 0, 0xFFFFFF80, 0xFFFFFF26, 0, 0});
}

+ (NSColor *)stroke {
    return Dynamic((Token){0xE2DDD4FF, 0x2E2C29FF, 0x9C9589FF, 0x5C5852FF, 0xFFFFFF99, 0xFFFFFF2E,
                           0x00000066, 0xFFFFFF80});
}

+ (NSColor *)track {
    return Dynamic((Token){0xE3DED5FF, 0x302E2BFF, 0xB3ACA0FF, 0x55514BFF, 0x0000001F, 0xFFFFFF38,
                           0x00000059, 0xFFFFFF66});
}

+ (NSColor *)textPrimary {
    return Dynamic((Token){0x1B1A18FF, 0xF2EFE9FF, 0x000000FF, 0xFFFFFFFF, 0x000000DB, 0xFFFFFFF5,
                           0x000000FF, 0xFFFFFFFF});
}

+ (NSColor *)textSecondary {
    return Dynamic((Token){0x57534BFF, 0xB7B2A8FF, 0x2F2C27FF, 0xDCD7CDFF, 0x00000094, 0xFFFFFFAD,
                           0x000000CC, 0xFFFFFFE6});
}

+ (NSColor *)textTertiary {
    return Dynamic((Token){0x75706AFF, 0x8C877EFF, 0x4A463FFF, 0xB7B2A8FF, 0x0000006B, 0xFFFFFF80,
                           0x000000B3, 0xFFFFFFCC});
}

+ (NSColor *)accent {
    return Dynamic(
        (Token){0xEC560CFF, 0xFF6B1FFF, 0xC24300FF, 0xFF7A33FF, 0x1D1D1FFF, 0xFFFFFFFF, 0, 0});
}

+ (NSColor *)accentInk {
    return Dynamic(
        (Token){0x1B0F05FF, 0x1B0F05FF, 0x000000FF, 0x000000FF, 0xFFFFFFFF, 0x1D1D1FFF, 0, 0});
}

+ (NSColor *)accentSoft {
    return Dynamic((Token){0xEC560C1F, 0xFF6B1F29, 0, 0, 0x0000000F, 0xFFFFFF1F, 0, 0});
}

+ (NSColor *)brand {
    return Dynamic(
        (Token){0xEC560CFF, 0xFF6B1FFF, 0xC24300FF, 0xFF7A33FF, 0xEC560CFF, 0xFF6B1FFF, 0, 0});
}

+ (NSColor *)colorForSwitchType:(NSString *)switchType {
    uint32_t light = 0x6B665CFF, dark = 0xB0AA9FFF; // other: stone
    if ([switchType isEqualToString:@"linear"]) {
        light = 0x3D875AFF, dark = 0x8CC19EFF;
    } else if ([switchType isEqualToString:@"tactile"]) {
        light = 0xA87719FF, dark = 0xE5B65EFF;
    } else if ([switchType isEqualToString:@"clicky"]) {
        light = 0x2E6BABFF, dark = 0x74AEE6FF;
    } else if ([switchType isEqualToString:@"topre"]) {
        light = 0x586575FF, dark = 0xA2AEBBFF;
    } else if ([switchType isEqualToString:@"buckling"]) {
        light = 0xA2502CFF, dark = 0xD88D67FF;
    }
    return Dynamic((Token){light, dark, 0, 0, light, dark, 0, 0});
}

+ (CGFloat)radius:(CGFloat)graphite {
    return g_style == KFThemeStyleGlass ? round(graphite * 1.6) : graphite;
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
