// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/*
 * audio-info: prints every audio device and its settings as JSON, so a snapshot taken before
 * and after running Keyfeel proves the app changed nothing (spec: Safety rules; Milestone 1
 * verifies the 128-frame buffer request does not persist or affect other apps).
 *
 * Read-only: it only calls AudioObjectGetPropertyData. It never opens, starts or configures a
 * device, so it never triggers a microphone prompt.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>

#define MAX_DEVICES 64u

static const char k_usage[] = "usage: audio-info\n"
                              "Prints every audio device and the defaults as JSON. Read-only.\n";

static bool get(AudioObjectID object, AudioObjectPropertySelector selector,
                AudioObjectPropertyScope scope, AudioObjectPropertyElement element, void *out,
                UInt32 size) {
    const AudioObjectPropertyAddress address = {selector, scope, element};
    if (!AudioObjectHasProperty(object, &address)) {
        return false;
    }
    UInt32 actual = size;
    return AudioObjectGetPropertyData(object, &address, 0u, NULL, &actual, out) == noErr &&
           actual == size;
}

static void print_json_string(const char *s) {
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)s; *p != '\0'; ++p) {
        if (*p == '"' || *p == '\\') {
            printf("\\%c", *p);
        } else if (*p < 0x20u) {
            printf("\\u%04x", *p);
        } else {
            putchar(*p);
        }
    }
    putchar('"');
}

static void print_string_property(AudioObjectID object, AudioObjectPropertySelector selector) {
    CFStringRef text = NULL;
    char buf[256] = "";
    if (get(object, selector, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain,
            &text, sizeof text) &&
        text != NULL) {
        (void)CFStringGetCString(text, buf, sizeof buf, kCFStringEncodingUTF8);
        CFRelease(text);
    }
    print_json_string(buf);
}

static AudioObjectID default_device(AudioObjectPropertySelector selector) {
    AudioObjectID id = kAudioObjectUnknown;
    (void)get(kAudioObjectSystemObject, selector, kAudioObjectPropertyScopeGlobal,
              kAudioObjectPropertyElementMain, &id, sizeof id);
    return id;
}

static bool has_streams(AudioObjectID device, AudioObjectPropertyScope scope) {
    const AudioObjectPropertyAddress address = {kAudioDevicePropertyStreams, scope,
                                                kAudioObjectPropertyElementMain};
    UInt32 size = 0u;
    return AudioObjectGetPropertyDataSize(device, &address, 0u, NULL, &size) == noErr && size > 0u;
}

static void print_scope(AudioObjectID device, AudioObjectPropertyScope scope, const char *name) {
    printf(",\n      \"%s\": {", name);
    Float32 volume = 0.0f;
    bool any = false;
    /* Main element first, then the first two channels: devices expose one or the other. */
    const AudioObjectPropertyElement elements[] = {kAudioObjectPropertyElementMain, 1u, 2u};
    for (size_t i = 0u; i < sizeof elements / sizeof elements[0]; ++i) {
        if (get(device, kAudioDevicePropertyVolumeScalar, scope, elements[i], &volume,
                sizeof volume)) {
            printf("%s\"volume_%u\": %.4f", any ? ", " : "", (unsigned)elements[i], (double)volume);
            any = true;
        }
    }
    UInt32 mute = 0u;
    if (get(device, kAudioDevicePropertyMute, scope, kAudioObjectPropertyElementMain, &mute,
            sizeof mute)) {
        printf("%s\"mute\": %s", any ? ", " : "", mute != 0u ? "true" : "false");
    }
    printf("}");
}

static void print_device(AudioObjectID device, bool first) {
    printf("%s    {\n      \"id\": %u,\n      \"name\": ", first ? "" : ",\n", (unsigned)device);
    print_string_property(device, kAudioObjectPropertyName);
    printf(",\n      \"uid\": ");
    print_string_property(device, kAudioDevicePropertyDeviceUID);

    UInt32 transport = 0u;
    char fourcc[5] = "----";
    if (get(device, kAudioDevicePropertyTransportType, kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain, &transport, sizeof transport)) {
        for (int i = 0; i < 4; ++i) {
            const char c = (char)((transport >> (24 - 8 * i)) & 0xffu);
            fourcc[i] = c >= 0x20 && c < 0x7f ? c : '?';
        }
    }
    printf(",\n      \"transport\": \"%s\"", fourcc);

    Float64 rate = 0.0;
    (void)get(device, kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal,
              kAudioObjectPropertyElementMain, &rate, sizeof rate);
    UInt32 frames = 0u;
    (void)get(device, kAudioDevicePropertyBufferFrameSize, kAudioObjectPropertyScopeGlobal,
              kAudioObjectPropertyElementMain, &frames, sizeof frames);
    AudioValueRange range = {0.0, 0.0};
    (void)get(device, kAudioDevicePropertyBufferFrameSizeRange, kAudioObjectPropertyScopeGlobal,
              kAudioObjectPropertyElementMain, &range, sizeof range);
    UInt32 running = 0u;
    (void)get(device, kAudioDevicePropertyDeviceIsRunningSomewhere, kAudioObjectPropertyScopeGlobal,
              kAudioObjectPropertyElementMain, &running, sizeof running);
    printf(",\n      \"nominal_sample_rate\": %.1f,\n      \"buffer_frame_size\": %u", rate,
           (unsigned)frames);
    printf(",\n      \"buffer_frame_size_range\": [%.0f, %.0f]", range.mMinimum, range.mMaximum);
    printf(",\n      \"running_somewhere\": %s", running != 0u ? "true" : "false");
    if (has_streams(device, kAudioObjectPropertyScopeOutput)) {
        print_scope(device, kAudioObjectPropertyScopeOutput, "output");
    }
    if (has_streams(device, kAudioObjectPropertyScopeInput)) {
        print_scope(device, kAudioObjectPropertyScopeInput, "input");
    }
    printf("\n    }");
}

int main(int argc, char **argv) {
    if (argc > 1) {
        fputs(k_usage,
              strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0 ? stdout : stderr);
        return strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0 ? EXIT_SUCCESS : 2;
    }
    AudioObjectID devices[MAX_DEVICES];
    const AudioObjectPropertyAddress list = {kAudioHardwarePropertyDevices,
                                             kAudioObjectPropertyScopeGlobal,
                                             kAudioObjectPropertyElementMain};
    UInt32 size = sizeof devices;
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &list, 0u, NULL, &size, devices) !=
        noErr) {
        fputs("audio-info: cannot list audio devices\n", stderr);
        return EXIT_FAILURE;
    }
    const UInt32 count = size / (UInt32)sizeof devices[0];
    printf("{\n  \"default_output\": %u,\n  \"default_system_output\": %u,\n"
           "  \"default_input\": %u,\n  \"devices\": [\n",
           (unsigned)default_device(kAudioHardwarePropertyDefaultOutputDevice),
           (unsigned)default_device(kAudioHardwarePropertyDefaultSystemOutputDevice),
           (unsigned)default_device(kAudioHardwarePropertyDefaultInputDevice));
    for (UInt32 i = 0u; i < count; ++i) {
        print_device(devices[i], i == 0u);
    }
    printf("\n  ]\n}\n");
    return ferror(stdout) ? EXIT_FAILURE : EXIT_SUCCESS;
}
