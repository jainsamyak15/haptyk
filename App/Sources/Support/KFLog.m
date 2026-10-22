// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "Support/KFLog.h"

static os_log_t Make(const char *category) {
    return os_log_create(KFBundleIdentifier().UTF8String, category);
}

os_log_t KFLogApp(void) {
    static os_log_t log;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        log = Make("app");
    });
    return log;
}

os_log_t KFLogPower(void) {
    static os_log_t log;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        log = Make("power");
    });
    return log;
}

os_log_t KFLogPacks(void) {
    static os_log_t log;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        log = Make("packs");
    });
    return log;
}

os_log_t KFLogStats(void) {
    static os_log_t log;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        log = Make("stats");
    });
    return log;
}

NSString *KFBundleIdentifier(void) {
    return NSBundle.mainBundle.bundleIdentifier ?: @"com.example.keyfeel";
}

static NSString *UserLibraryFolder(NSSearchPathDirectory directory, NSString *fallback) {
    return NSSearchPathForDirectoriesInDomains(directory, NSUserDomainMask, YES).firstObject
               ?: [NSHomeDirectory() stringByAppendingPathComponent:fallback];
}

static NSString *Created(NSString *path) {
    [NSFileManager.defaultManager createDirectoryAtPath:path
                            withIntermediateDirectories:YES
                                             attributes:nil
                                                  error:NULL];
    return path;
}

NSString *KFApplicationSupportPath(void) {
    return Created([UserLibraryFolder(NSApplicationSupportDirectory, @"Library/Application Support")
        stringByAppendingPathComponent:@"Keyfeel"]);
}

NSURL *KFApplicationSupportURL(void) {
    return [NSURL fileURLWithPath:KFApplicationSupportPath() isDirectory:YES];
}

NSURL *KFApplicationSupportSubdirectory(NSString *name) {
    return [NSURL
        fileURLWithPath:Created([KFApplicationSupportPath() stringByAppendingPathComponent:name])
            isDirectory:YES];
}

NSString *KFPath(NSURL *_Nullable url) {
    return url.path ?: @"";
}

NSString *KFCachesPath(void) {
    return [UserLibraryFolder(NSCachesDirectory, @"Library/Caches")
        stringByAppendingPathComponent:KFBundleIdentifier()];
}
