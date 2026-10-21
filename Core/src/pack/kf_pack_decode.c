// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/* Pack decoding with ExtAudioFile (macOS). Runs on a background thread, never real-time. */

#include <kf/kf_pack.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <kf/kf_config.h>

#include "pack/kf_pack_internal.h"

#if defined(__APPLE__)

#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>

#define KF_DECODE_MARGIN_FRAMES 16u /* Room for the converter's rounding. */

static const char *base_name(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash != NULL ? slash + 1 : path;
}

static ExtAudioFileRef open_file(const char *path) {
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, (const UInt8 *)path,
                                                           (CFIndex)strlen(path), false);
    if (url == NULL) {
        return NULL;
    }
    ExtAudioFileRef file = NULL;
    const OSStatus status = ExtAudioFileOpenURL(url, &file);
    CFRelease(url);
    return status == noErr ? file : NULL;
}

/* Checks container, encoding, channels, rate and length; returns the decoded frame count at
 * device_rate (an upper bound), or 0 with a message. */
static uint32_t probe(ExtAudioFileRef file, const char *path, double device_rate, char *err,
                      size_t errlen) {
    const char *name = base_name(path);
    AudioFileID audio_file = NULL;
    UInt32 size = sizeof audio_file;
    UInt32 type = 0u;
    UInt32 type_size = sizeof type;
    if (ExtAudioFileGetProperty(file, kExtAudioFileProperty_AudioFile, &size, &audio_file) !=
            noErr ||
        AudioFileGetProperty(audio_file, kAudioFilePropertyFileFormat, &type_size, &type) !=
            noErr ||
        (type != kAudioFileWAVEType && type != kAudioFileCAFType)) {
        (void)kf_pack_fail(err, errlen, "%s: only WAV and CAF files are supported", name);
        return 0u;
    }
    AudioStreamBasicDescription format;
    size = sizeof format;
    if (ExtAudioFileGetProperty(file, kExtAudioFileProperty_FileDataFormat, &size, &format) !=
        noErr) {
        (void)kf_pack_fail(err, errlen, "%s: unreadable audio format", name);
        return 0u;
    }
    const bool pcm = format.mFormatID == kAudioFormatLinearPCM &&
                     (format.mFormatFlags & kAudioFormatFlagIsFloat) == 0u &&
                     (format.mBitsPerChannel == 16u || format.mBitsPerChannel == 24u);
    if (!pcm && format.mFormatID != kAudioFormatAppleLossless) {
        (void)kf_pack_fail(err, errlen, "%s: needs 16- or 24-bit PCM or ALAC", name);
        return 0u;
    }
    if (format.mChannelsPerFrame != 1u) {
        (void)kf_pack_fail(err, errlen, "%s: has %u channels; packs need mono files", name,
                           (unsigned)format.mChannelsPerFrame);
        return 0u;
    }
    if (format.mSampleRate != 44100.0 && format.mSampleRate != 48000.0) {
        (void)kf_pack_fail(err, errlen, "%s: %.0f Hz; packs need 44.1 or 48 kHz", name,
                           format.mSampleRate);
        return 0u;
    }
    SInt64 frames = 0;
    size = sizeof frames;
    if (ExtAudioFileGetProperty(file, kExtAudioFileProperty_FileLengthFrames, &size, &frames) !=
            noErr ||
        frames <= 0) {
        (void)kf_pack_fail(err, errlen, "%s: holds no audio", name);
        return 0u;
    }
    if ((double)frames * 1000.0 > format.mSampleRate * (double)KF_PACK_MAX_FILE_MS) {
        (void)kf_pack_fail(err, errlen, "%s: longer than %u ms", name,
                           (unsigned)KF_PACK_MAX_FILE_MS);
        return 0u;
    }
    return (uint32_t)ceil((double)frames * device_rate / format.mSampleRate) +
           KF_DECODE_MARGIN_FRAMES;
}

/* Decodes into dst (capacity frames) as 16-bit mono at device_rate, best-quality resampling.
 * Writes every frame of dst, so all its pages are touched. Returns the frames decoded. */
static uint32_t decode(ExtAudioFileRef file, double device_rate, int16_t *dst, uint32_t capacity,
                       const char *path, char *err, size_t errlen) {
    AudioStreamBasicDescription client = {
        .mSampleRate = device_rate,
        .mFormatID = kAudioFormatLinearPCM,
        .mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked,
        .mBytesPerPacket = 2u,
        .mFramesPerPacket = 1u,
        .mBytesPerFrame = 2u,
        .mChannelsPerFrame = 1u,
        .mBitsPerChannel = 16u,
    };
    if (ExtAudioFileSetProperty(file, kExtAudioFileProperty_ClientDataFormat, sizeof client,
                                &client) != noErr) {
        (void)kf_pack_fail(err, errlen, "%s: cannot convert to 16-bit mono", base_name(path));
        return 0u;
    }
    AudioConverterRef converter = NULL;
    UInt32 size = sizeof converter;
    if (ExtAudioFileGetProperty(file, kExtAudioFileProperty_AudioConverter, &size, &converter) ==
            noErr &&
        converter != NULL) {
        const UInt32 quality = kAudioConverterQuality_Max;
        if (AudioConverterSetProperty(converter, kAudioConverterSampleRateConverterQuality,
                                      sizeof quality, &quality) == noErr) {
            CFArrayRef config = NULL; /* Tells ExtAudioFile the converter changed. */
            (void)ExtAudioFileSetProperty(file, kExtAudioFileProperty_ConverterConfig,
                                          sizeof config, &config);
        }
    }
    uint32_t done = 0u;
    while (done < capacity) {
        AudioBufferList list = {
            .mNumberBuffers = 1u,
            .mBuffers = {{1u, (UInt32)((capacity - done) * sizeof(int16_t)), dst + done}},
        };
        UInt32 frames = capacity - done;
        if (ExtAudioFileRead(file, &frames, &list) != noErr) {
            (void)kf_pack_fail(err, errlen, "%s: decoding failed", base_name(path));
            return 0u;
        }
        if (frames == 0u) {
            break;
        }
        done += frames;
    }
    memset(dst + done, 0, (size_t)(capacity - done) * sizeof *dst);
    if (done == 0u) {
        (void)kf_pack_fail(err, errlen, "%s: holds no audio", base_name(path));
    }
    return done;
}

kf_pack *kf_pack_load(const kf_pack_file *files, size_t count, double device_rate, char *err,
                      size_t errlen) {
    if (files == NULL || count == 0u) {
        return kf_pack_fail(err, errlen, "no files given");
    }
    if (count > KF_PACK_MAX_FILES) {
        return kf_pack_fail(err, errlen, "%zu files; a pack holds at most %u", count,
                            (unsigned)KF_PACK_MAX_FILES);
    }
    if (!(device_rate >= 8000.0 && device_rate <= 384000.0)) {
        return kf_pack_fail(err, errlen, "unsupported device rate %.0f Hz", device_rate);
    }

    /* Pass 1: validate every file and size the block, so nothing is allocated for a pack
     * that will be rejected. Files are closed between passes: 400 open files would exceed
     * the default descriptor limit. */
    uint32_t *capacity = malloc(count * sizeof *capacity);
    kf_pack_entry *entries = malloc(count * sizeof *entries);
    if (capacity == NULL || entries == NULL) {
        free(capacity);
        free(entries);
        return kf_pack_fail(err, errlen, "out of memory");
    }
    size_t total = 0u;
    for (size_t i = 0u; i < count; ++i) {
        const kf_pack_file *f = &files[i];
        if (f->path == NULL || !kf_pack_entry_is_valid(f->group, f->is_up, f->tier)) {
            free(capacity);
            free(entries);
            return kf_pack_fail(err, errlen, "file %zu: invalid group, tier or path", i);
        }
        ExtAudioFileRef file = open_file(f->path);
        if (file == NULL) {
            free(capacity);
            free(entries);
            return kf_pack_fail(err, errlen, "%s: cannot open as audio", base_name(f->path));
        }
        capacity[i] = probe(file, f->path, device_rate, err, errlen);
        (void)ExtAudioFileDispose(file);
        if (capacity[i] == 0u) {
            free(capacity);
            free(entries);
            return NULL;
        }
        total += capacity[i];
    }
    if (total * sizeof(int16_t) > KF_PACK_MAX_DECODED_BYTES) {
        free(capacity);
        free(entries);
        return kf_pack_fail(err, errlen, "decodes to %zu KB; a pack may use at most %u KB",
                            total * sizeof(int16_t) / 1024u,
                            (unsigned)(KF_PACK_MAX_DECODED_BYTES / 1024u));
    }

    /* Pass 2: decode into one block. */
    int16_t *block = malloc(total * sizeof *block);
    if (block == NULL) {
        free(capacity);
        free(entries);
        return kf_pack_fail(err, errlen, "out of memory");
    }
    size_t offset = 0u;
    for (size_t i = 0u; i < count; ++i) {
        const kf_pack_file *f = &files[i];
        ExtAudioFileRef file = open_file(f->path);
        const uint32_t frames = file != NULL ? decode(file, device_rate, block + offset,
                                                      capacity[i], f->path, err, errlen)
                                             : 0u;
        if (file != NULL) {
            (void)ExtAudioFileDispose(file);
        } else {
            (void)kf_pack_fail(err, errlen, "%s: cannot open as audio", base_name(f->path));
        }
        if (frames == 0u) {
            free(block);
            free(capacity);
            free(entries);
            return NULL;
        }
        entries[i] = (kf_pack_entry){f->group, f->is_up, f->tier, f->derived, offset, frames};
        offset += capacity[i];
    }
    free(capacity);
    kf_pack *pack = kf_pack_build(block, total, entries, count, device_rate, err, errlen);
    free(entries);
    return pack;
}

#else /* Host builds: the portable parts are tested; decoding needs macOS. */

kf_pack *kf_pack_load(const kf_pack_file *files, size_t count, double device_rate, char *err,
                      size_t errlen) {
    (void)files;
    (void)count;
    (void)device_rate;
    return kf_pack_fail(err, errlen, "decoding audio files needs macOS");
}

#endif
