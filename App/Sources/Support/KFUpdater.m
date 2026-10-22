// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "Support/KFUpdater.h"

#import <objc/message.h>

#import "Support/KFLog.h"

@implementation KFUpdater {
    id _controller; /* SPUStandardUpdaterController, looked up at run time. */
}

- (BOOL)available {
    return _controller != nil;
}

- (nullable NSString *)configurationProblem {
    NSDictionary *info = NSBundle.mainBundle.infoDictionary;
    NSString *feed = info[@"SUFeedURL"];
    NSString *key = info[@"SUPublicEDKey"];
    if (feed.length == 0 || [feed containsString:@"example.com"] || ![feed hasPrefix:@"https://"]) {
        return @"This build has no update feed configured.";
    }
    if (key.length == 0 || [key hasPrefix:@"REPLACE"]) {
        return @"This build has no update signing key configured.";
    }
    return nil;
}

- (void)start {
    NSString *problem = [self configurationProblem];
    if (problem != nil) {
        _unavailableReason = problem;
        return;
    }
    NSURL *frameworks = NSBundle.mainBundle.privateFrameworksURL;
    NSBundle *sparkle =
        frameworks != nil
            ? [NSBundle bundleWithURL:(NSURL *_Nonnull)[frameworks
                                          URLByAppendingPathComponent:@"Sparkle.framework"]]
            : nil;
    NSError *error = nil;
    if (sparkle == nil || ![sparkle loadAndReturnError:&error]) {
        _unavailableReason = @"Updates are part of the signed release build.";
        return;
    }
    Class controllerClass = NSClassFromString(@"SPUStandardUpdaterController");
    SEL init = NSSelectorFromString(@"initWithStartingUpdater:updaterDelegate:userDriverDelegate:");
    if (controllerClass == Nil || ![controllerClass instancesRespondToSelector:init]) {
        _unavailableReason = @"The embedded Sparkle is not a supported version.";
        return;
    }
    id (*initialize)(id, SEL, BOOL, id, id) = (id (*)(id, SEL, BOOL, id, id))objc_msgSend;
    _controller = initialize([controllerClass alloc], init, YES, nil, nil);
    _unavailableReason = _controller == nil ? @"Sparkle could not start." : nil;
    os_log_info(KFLogApp(), "updates %{public}s", _controller != nil ? "on" : "off");
}

- (void)checkForUpdates {
    SEL check = NSSelectorFromString(@"checkForUpdates:");
    if ([_controller respondsToSelector:check]) {
        void (*send)(id, SEL, id) = (void (*)(id, SEL, id))objc_msgSend;
        send(_controller, check, nil);
    }
}

@end
