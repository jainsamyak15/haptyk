// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "Menu/KFVolumeMenuView.h"

#import "Support/KFUI.h"

@implementation KFVolumeMenuView {
    NSSlider *_slider;
}

- (instancetype)init {
    if ((self = [super initWithFrame:NSMakeRect(0, 0, 260, 30)])) {
        NSImageView *low = [NSImageView imageViewWithImage:KFSymbol(@"speaker.fill", nil)];
        NSImageView *high = [NSImageView imageViewWithImage:KFSymbol(@"speaker.wave.3.fill", nil)];
        low.contentTintColor = high.contentTintColor = NSColor.secondaryLabelColor;
        _slider = [NSSlider sliderWithValue:0.7
                                   minValue:0.0
                                   maxValue:1.0
                                     target:self
                                     action:@selector(slid:)];
        _slider.continuous = YES;
        _slider.accessibilityLabel = @"Volume";
        NSStackView *row = [NSStackView stackViewWithViews:@[ low, _slider, high ]];
        row.spacing = 8.0;
        row.edgeInsets = NSEdgeInsetsMake(4, 20, 4, 14);
        row.translatesAutoresizingMaskIntoConstraints = NO;
        [self addSubview:row];
        [NSLayoutConstraint activateConstraints:@[
            [row.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
            [row.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
            [row.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
        ]];
    }
    return self;
}

- (float)volume {
    return (float)_slider.doubleValue;
}

- (void)setVolume:(float)volume {
    _slider.doubleValue = volume;
}

- (void)slid:(NSSlider *)sender {
    if (self.onChange != nil) {
        self.onChange((float)sender.doubleValue);
    }
}

@end
