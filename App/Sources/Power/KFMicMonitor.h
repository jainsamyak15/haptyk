// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// "Mute while the microphone is in use" (spec: Power state machine). Event-driven through
/// CoreAudio property listeners, never polling, and the app never opens the microphone: it
/// only reads whether another process runs input. On macOS 14+ that is exact, per process
/// (kAudioProcessPropertyIsRunningInput), and only input from a real microphone counts: a
/// physical device, or an aggregate built on one. Virtual devices, system-audio capture and
/// screen recorders don't mute Keyfeel. On macOS 13 it falls back to the default input
/// device running anywhere, ignored when that device is also the output Keyfeel plays on.
/// Main thread.
@interface KFMicMonitor : NSObject

@property (nonatomic, readonly) BOOL microphoneInUse;
/// Called on the main queue when microphoneInUse changes.
@property (nonatomic, copy, nullable) void (^onChange)(BOOL inUse);

- (void)start;
- (void)stop;

@end

/// Whether a device with this CoreAudio transport type can be a microphone. Unknown, virtual
/// and aggregate devices can't on their own: their input may be another app's output.
BOOL KFMicTransportIsPhysical(UInt32 transportType);

NS_ASSUME_NONNULL_END
