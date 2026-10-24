// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "Power/KFMicMonitor.h"

#import <CoreAudio/CoreAudio.h>
#include <unistd.h>

static const AudioObjectPropertyAddress kDefaultInput = {kAudioHardwarePropertyDefaultInputDevice,
                                                         kAudioObjectPropertyScopeGlobal,
                                                         kAudioObjectPropertyElementMain};
static const AudioObjectPropertyAddress kDefaultOutput = {kAudioHardwarePropertyDefaultOutputDevice,
                                                          kAudioObjectPropertyScopeGlobal,
                                                          kAudioObjectPropertyElementMain};
static const AudioObjectPropertyAddress kRunningSomewhere = {
    kAudioDevicePropertyDeviceIsRunningSomewhere, kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMain};
static const AudioObjectPropertyAddress kProcessList = {kAudioHardwarePropertyProcessObjectList,
                                                        kAudioObjectPropertyScopeGlobal,
                                                        kAudioObjectPropertyElementMain};
static const AudioObjectPropertyAddress kProcessRunningInput = {kAudioProcessPropertyIsRunningInput,
                                                                kAudioObjectPropertyScopeGlobal,
                                                                kAudioObjectPropertyElementMain};
static const AudioObjectPropertyAddress kProcessPID = {
    kAudioProcessPropertyPID, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
static const AudioObjectPropertyAddress kProcessInputDevices = {
    kAudioProcessPropertyDevices, kAudioObjectPropertyScopeInput, kAudioObjectPropertyElementMain};
static const AudioObjectPropertyAddress kTransport = {kAudioDevicePropertyTransportType,
                                                      kAudioObjectPropertyScopeGlobal,
                                                      kAudioObjectPropertyElementMain};
static const AudioObjectPropertyAddress kInputStreams = {
    kAudioDevicePropertyStreams, kAudioObjectPropertyScopeInput, kAudioObjectPropertyElementMain};
static const AudioObjectPropertyAddress kSubDevices = {
    kAudioAggregateDevicePropertyActiveSubDeviceList, kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMain};

BOOL KFMicTransportIsPhysical(UInt32 transportType) {
    switch (transportType) {
    case kAudioDeviceTransportTypeUnknown:
    case kAudioDeviceTransportTypeVirtual:
    case kAudioDeviceTransportTypeAggregate:
    case kAudioDeviceTransportTypeAutoAggregate:
        return NO;
    default:
        return YES;
    }
}

static AudioObjectID DefaultDevice(const AudioObjectPropertyAddress *address) {
    AudioObjectID device = kAudioObjectUnknown;
    UInt32 size = sizeof device;
    (void)AudioObjectGetPropertyData(kAudioObjectSystemObject, address, 0, NULL, &size, &device);
    return device;
}

static UInt32 ReadUInt32(AudioObjectID object, const AudioObjectPropertyAddress *address) {
    UInt32 value = 0;
    UInt32 size = sizeof value;
    if (!AudioObjectHasProperty(object, address) ||
        AudioObjectGetPropertyData(object, address, 0, NULL, &size, &value) != noErr) {
        return 0;
    }
    return value;
}

static NSArray<NSNumber *> *ReadObjects(AudioObjectID object,
                                        const AudioObjectPropertyAddress *address) {
    UInt32 size = 0;
    if (!AudioObjectHasProperty(object, address) ||
        AudioObjectGetPropertyDataSize(object, address, 0, NULL, &size) != noErr || size == 0) {
        return @[];
    }
    NSMutableData *data = [NSMutableData dataWithLength:size];
    if (AudioObjectGetPropertyData(object, address, 0, NULL, &size, data.mutableBytes) != noErr) {
        return @[];
    }
    const AudioObjectID *ids = data.bytes;
    NSMutableArray<NSNumber *> *objects = [NSMutableArray array];
    for (UInt32 i = 0; i < size / sizeof(AudioObjectID); ++i) {
        [objects addObject:@(ids[i])];
    }
    return objects;
}

/// A real microphone: a physical device with input streams, or an aggregate built on one.
/// `depth` stops aggregates of aggregates from looping.
static BOOL IsMicrophone(AudioObjectID device, int depth) {
    const UInt32 transport = ReadUInt32(device, &kTransport);
    if (transport == kAudioDeviceTransportTypeAggregate ||
        transport == kAudioDeviceTransportTypeAutoAggregate) {
        if (depth >= 3) {
            return NO;
        }
        for (NSNumber *sub in ReadObjects(device, &kSubDevices)) {
            if (IsMicrophone(sub.unsignedIntValue, depth + 1)) {
                return YES;
            }
        }
        return NO;
    }
    UInt32 size = 0;
    return KFMicTransportIsPhysical(transport) &&
           AudioObjectGetPropertyDataSize(device, &kInputStreams, 0, NULL, &size) == noErr &&
           size > 0;
}

/// Whether a process running input records from a microphone. Virtual devices, system-audio
/// capture and screen recorders don't count. When CoreAudio lists no input devices for it, it
/// counts: better quiet during a call than clacking into it.
static BOOL UsesMicrophone(AudioObjectID process) {
    NSArray<NSNumber *> *devices = ReadObjects(process, &kProcessInputDevices);
    if (devices.count == 0) {
        return YES;
    }
    for (NSNumber *device in devices) {
        if (IsMicrophone(device.unsignedIntValue, 0)) {
            return YES;
        }
    }
    return NO;
}

@interface KFMicMonitor ()
@property (nonatomic, readwrite) BOOL microphoneInUse;
@end

@implementation KFMicMonitor {
    BOOL _running;
    AudioObjectID _device;
    NSArray<NSNumber *> *_processes;
    AudioObjectPropertyListenerBlock _listener;
}

- (instancetype)init {
    if ((self = [super init])) {
        _device = kAudioObjectUnknown;
        _processes = @[];
        __weak KFMicMonitor *weakSelf = self;
        /* One block for every listener, delivered on the main queue. */
        _listener = ^(UInt32 count, const AudioObjectPropertyAddress *addresses) {
            (void)count;
            (void)addresses;
            [weakSelf refresh];
        };
    }
    return self;
}

- (void)dealloc {
    [self stop];
}

- (BOOL)hasProcessAPI {
    return AudioObjectHasProperty(kAudioObjectSystemObject, &kProcessList);
}

- (void)start {
    if (_running) {
        return;
    }
    _running = YES;
    dispatch_queue_t main = dispatch_get_main_queue();
    (void)AudioObjectAddPropertyListenerBlock(kAudioObjectSystemObject, &kDefaultInput, main,
                                              _listener);
    (void)AudioObjectAddPropertyListenerBlock(kAudioObjectSystemObject, &kDefaultOutput, main,
                                              _listener);
    if (self.hasProcessAPI) {
        (void)AudioObjectAddPropertyListenerBlock(kAudioObjectSystemObject, &kProcessList, main,
                                                  _listener);
    }
    [self refresh];
}

- (void)stop {
    if (!_running) {
        return;
    }
    _running = NO;
    dispatch_queue_t main = dispatch_get_main_queue();
    (void)AudioObjectRemovePropertyListenerBlock(kAudioObjectSystemObject, &kDefaultInput, main,
                                                 _listener);
    (void)AudioObjectRemovePropertyListenerBlock(kAudioObjectSystemObject, &kDefaultOutput, main,
                                                 _listener);
    if (self.hasProcessAPI) {
        (void)AudioObjectRemovePropertyListenerBlock(kAudioObjectSystemObject, &kProcessList, main,
                                                     _listener);
    }
    [self watchDevice:kAudioObjectUnknown];
    [self watchProcesses:@[]];
    self.microphoneInUse = NO;
}

- (void)watchDevice:(AudioObjectID)device {
    if (device == _device) {
        return;
    }
    dispatch_queue_t main = dispatch_get_main_queue();
    if (_device != kAudioObjectUnknown) {
        (void)AudioObjectRemovePropertyListenerBlock(_device, &kRunningSomewhere, main, _listener);
    }
    _device = device;
    if (_device != kAudioObjectUnknown) {
        (void)AudioObjectAddPropertyListenerBlock(_device, &kRunningSomewhere, main, _listener);
    }
}

/// Per-process listeners, so input starting on a device that already runs, or moving to
/// another device, is still noticed.
- (void)watchProcesses:(NSArray<NSNumber *> *)processes {
    dispatch_queue_t main = dispatch_get_main_queue();
    for (NSNumber *process in _processes) {
        if (![processes containsObject:process]) {
            (void)AudioObjectRemovePropertyListenerBlock(process.unsignedIntValue,
                                                         &kProcessRunningInput, main, _listener);
            (void)AudioObjectRemovePropertyListenerBlock(process.unsignedIntValue,
                                                         &kProcessInputDevices, main, _listener);
        }
    }
    for (NSNumber *process in processes) {
        if (![_processes containsObject:process]) {
            (void)AudioObjectAddPropertyListenerBlock(process.unsignedIntValue,
                                                      &kProcessRunningInput, main, _listener);
            (void)AudioObjectAddPropertyListenerBlock(process.unsignedIntValue,
                                                      &kProcessInputDevices, main, _listener);
        }
    }
    _processes = [processes copy];
}

- (void)refresh {
    if (!_running) {
        return;
    }
    const AudioObjectID input = DefaultDevice(&kDefaultInput);
    [self watchDevice:input];
    BOOL inUse = NO;
    if (self.hasProcessAPI) {
        NSArray<NSNumber *> *processes = ReadObjects(kAudioObjectSystemObject, &kProcessList);
        [self watchProcesses:processes];
        const pid_t me = getpid();
        for (NSNumber *process in processes) {
            const AudioObjectID object = process.unsignedIntValue;
            if ((pid_t)ReadUInt32(object, &kProcessPID) != me &&
                ReadUInt32(object, &kProcessRunningInput) != 0 && UsesMicrophone(object)) {
                inUse = YES;
                break;
            }
        }
    } else if (input != kAudioObjectUnknown && input != DefaultDevice(&kDefaultOutput)) {
        inUse = ReadUInt32(input, &kRunningSomewhere) != 0;
    }
    if (inUse != self.microphoneInUse) {
        self.microphoneInUse = inUse;
        if (self.onChange != nil) {
            self.onChange(inUse);
        }
    }
}

@end
