// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import <Foundation/Foundation.h>

#import "Packs/KFPackManifest.h"

NS_ASSUME_NONNULL_BEGIN

/// Posted on the main queue when the pack list or the loaded pack changes.
extern NSNotificationName const KFPackLibraryDidChangeNotification;

/// Bundled packs (Contents/Resources/Packs) and user packs (~/Library/Application Support/
/// Keyfeel/Packs): listing, loading into the engine, import and delete. Main thread, except
/// that decoding runs on a background queue.
@interface KFPackLibrary : NSObject

- (instancetype)initWithBundledURL:(nullable NSURL *)bundledURL
                           userURL:(NSURL *)userURL NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

/// Bundled packs in the spec's order, then user packs by name. Invalid packs are skipped and
/// logged, never shown.
@property (nonatomic, readonly, copy) NSArray<KFPackManifest *> *packs;
@property (nonatomic, readonly) NSURL *userURL;
- (BOOL)isBundled:(KFPackManifest *)pack;
- (nullable KFPackManifest *)packWithIdentifier:(NSString *)identifier;
/// The pack for `identifier`, else the default: the first linear pack, else the first pack.
- (nullable KFPackManifest *)resolvePack:(nullable NSString *)identifier;
- (void)reload;

/// The pack in the engine and the rate it was decoded at (0 when none).
@property (nonatomic, readonly, nullable) KFPackManifest *loadedPack;
@property (nonatomic, readonly) double loadedRate;

/// Decodes `pack` at `rate` on a background queue, hands it to the engine on the main queue,
/// then calls `completion` there (nil error on success).
- (void)loadPack:(KFPackManifest *)pack
            rate:(double)rate
      completion:(nullable void (^)(NSError *_Nullable error))completion;
/// The same, on the calling thread: for a device-rate change right before audio starts.
- (BOOL)loadPackNow:(KFPackManifest *)pack rate:(double)rate error:(NSError **)error;

/// Imports a .kfpack folder or a .zip holding one. Extracts a zip with the built-in ditto into
/// the app's caches folder, validates everything (manifest, paths, no symlinks, size limits,
/// a trial decode) and only then copies it into the user packs folder. Nothing from a pack is
/// ever executed. Completion on the main queue.
- (void)importPackAtURL:(NSURL *)url
             completion:
                 (void (^)(KFPackManifest *_Nullable pack, NSError *_Nullable error))completion;

/// Deletes a user pack. Bundled packs cannot be deleted.
- (BOOL)deletePack:(KFPackManifest *)pack error:(NSError **)error;

/// Attribution lines every license requires, for the About window.
- (NSArray<NSString *> *)attributions;

@end

NS_ASSUME_NONNULL_END
