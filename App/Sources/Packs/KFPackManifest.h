// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import <Foundation/Foundation.h>

#include <kf/kf_pack.h>

NS_ASSUME_NONNULL_BEGIN

extern NSErrorDomain const KFPackErrorDomain;

typedef NS_ERROR_ENUM(KFPackErrorDomain, KFPackError){
    KFPackErrorUnreadable = 1, ///< pack.json missing, too large or not a regular file.
    KFPackErrorMalformed,      ///< Not JSON, or a field is missing or of the wrong type.
    KFPackErrorUnsafePath,     ///< A path leaves the pack, is absolute, or crosses a symlink.
    KFPackErrorMissingFile,    ///< A listed audio file does not exist.
    KFPackErrorLimit,          ///< Too many or too large files.
    KFPackErrorDecode,         ///< kf_pack_load() rejected the audio.
};

/// A validated `pack.json` (docs/SPEC.md, "Sound pack format"; schema in Packs/README.md).
/// The core never parses JSON (ADR-0006): this class turns a pack folder into the flat file
/// list that kf_pack_load() decodes. Thread-safe once created.
@interface KFPackManifest : NSObject

@property (nonatomic, readonly) NSURL *packURL;
@property (nonatomic, readonly, copy) NSString *identifier;
@property (nonatomic, readonly, copy) NSString *name;
@property (nonatomic, readonly, copy) NSString *switchType;
@property (nonatomic, readonly, copy) NSString *author;
@property (nonatomic, readonly, copy) NSString *license;
@property (nonatomic, readonly, copy) NSString *version;
@property (nonatomic, readonly, copy, nullable) NSString *sourceURL;
@property (nonatomic, readonly, copy) NSString *attribution;
@property (nonatomic, readonly) NSUInteger fileCount;
/// True when any tier was generated rather than recorded.
@property (nonatomic, readonly) BOOL hasDerivedTiers;

/// Switch types a manifest may name, in display order.
@property (class, nonatomic, readonly) NSArray<NSString *> *switchTypes;

/// Reads and validates `packURL/pack.json` and every file it lists. Cheap: no audio is
/// decoded. Returns nil with a readable KFPackErrorDomain error naming the offending field.
+ (nullable instancetype)manifestWithPackAtURL:(NSURL *)packURL error:(NSError **)error;

- (instancetype)init NS_UNAVAILABLE;

/// Decodes the pack at `sampleRate` (kf_pack_load). Background queue only: takes tens of
/// milliseconds. The caller owns the result and hands it to kf_engine_set_pack() or frees it.
- (nullable kf_pack *)loadPackAtSampleRate:(double)sampleRate error:(NSError **)error;
/// The same, plus Big Enter's boom (KF_SOUND_BOOM) when `boomURL` is set.
- (nullable kf_pack *)loadPackAtSampleRate:(double)sampleRate
                                      boom:(nullable NSURL *)boomURL
                                     error:(NSError **)error;

@end

NS_ASSUME_NONNULL_END
