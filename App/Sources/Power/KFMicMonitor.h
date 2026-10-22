// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// "Mute while the microphone is in use" (spec: Power state machine). Event-driven through
/// CoreAudio property listeners, never polling, and the app never opens the microphone: it
/// only reads whether another process runs input. On macOS 14+ that is exact, per process
/// (kAudioProcessPropertyIsRunningInput). On macOS 13 it falls back to the default input
/// device running anywhere, ignored when that device is also the output Keyfeel plays on.
/// Main thread.
@interface KFMicMonitor : NSObject

@property (nonatomic, readonly) BOOL microphoneInUse;
/// Called on the main queue when microphoneInUse changes.
@property (nonatomic, copy, nullable) void (^onChange)(BOOL inUse);

- (void)start;
- (void)stop;

@end

NS_ASSUME_NONNULL_END
