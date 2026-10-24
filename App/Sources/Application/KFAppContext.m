// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "Application/KFAppContext.h"

#include <math.h>

#import "Power/KFPowerController.h"
#import "Stats/KFBadge.h"
#import "Stats/KFStatsController.h"
#import "Telemetry/KFTelemetry.h"

static const NSUInteger kBadgeDays = 7;

@implementation KFAppContext

- (NSUInteger)measuredKeystrokes {
    NSUInteger total = 0;
    for (NSNumber *n in [self.stats histogramForLastDays:kBadgeDays]) {
        total += n.unsignedIntegerValue;
    }
    return total;
}

/// The user's force is their 7-day median, compared locally with the downloaded table
/// (spec: "Percentile and badges"). Nothing about the user is sent for this.
- (BOOL)currentPercentile:(double *)percentile badge:(NSInteger *)badge {
    NSArray<NSNumber *> *table = self.telemetry.percentileTable;
    if (table == nil || self.measuredKeystrokes < KFBadge.requiredKeystrokes) {
        return NO;
    }
    const double force =
        [KFBadge medianForceOfHistogram:[self.stats histogramForLastDays:kBadgeDays]];
    const double p = [KFBadge percentileOfForce:force table:table];
    if (!isfinite(p)) {
        return NO;
    }
    *percentile = p;
    *badge = [KFBadge badgeForPercentile:p];
    return YES;
}

- (nullable NSString *)badgeLine {
    if (self.power.forceStatus == KFForceStatusUnavailable ||
        self.power.forceStatus == KFForceStatusDisabled || self.telemetry.percentileTable == nil) {
        return nil; /* No force data, or no network: hide the line. */
    }
    double percentile = 0.0;
    NSInteger badge = 0;
    if (![self currentPercentile:&percentile badge:&badge]) {
        return @"Keep typing to earn your badge";
    }
    return [NSString stringWithFormat:@"Harder than %.0f%% of users · %@", percentile,
                                      [KFBadge nameOfBadge:(KFBadgeKind)badge]];
}

@end
