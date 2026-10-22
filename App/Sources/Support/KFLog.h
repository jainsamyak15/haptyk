// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import <Foundation/Foundation.h>
#import <os/log.h>

NS_ASSUME_NONNULL_BEGIN

/// os_log categories under the app's subsystem. Never log key data or typed text, and never
/// log from the audio, sensor or keytap threads (CLAUDE.md, "Privacy rules").
os_log_t KFLogApp(void);
os_log_t KFLogPower(void);
os_log_t KFLogPacks(void);
os_log_t KFLogStats(void);

/// The app's bundle identifier, or the default until the owner chooses one.
NSString *KFBundleIdentifier(void);

/// ~/Library/Application Support/Keyfeel, created on first use. The only folder the app writes
/// besides its caches and its preferences domain.
NSString *KFApplicationSupportPath(void);
NSURL *KFApplicationSupportURL(void);
/// A folder inside it (created on first use), e.g. "Packs" or "stats".
NSURL *KFApplicationSupportSubdirectory(NSString *name);
/// ~/Library/Caches/<bundle id>: scratch space, e.g. for pack imports.
NSString *KFCachesPath(void);
/// A file URL's path. Never nil for file URLs; "" otherwise.
NSString *KFPath(NSURL *_Nullable url);

NS_ASSUME_NONNULL_END
