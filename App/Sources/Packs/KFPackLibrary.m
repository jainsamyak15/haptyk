// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "Packs/KFPackLibrary.h"

#include <sys/stat.h>

#include <kf/kf_engine.h>

#import "Support/KFLog.h"

NSNotificationName const KFPackLibraryDidChangeNotification = @"KFPackLibraryDidChange";

/// Bundled packs in the spec's order ("Bundled packs"); names here match pack.json names.
static NSArray<NSString *> *SpecOrder(void) {
    return @[
        @"Alpaca", @"Biscuit", @"Blue Alps", @"Buckling Spring", @"Cherry MX Black PBT",
        @"Cherry MX Blue PBT", @"Cherry MX Brown PBT", @"Cherry MX Red PBT", @"EG Crystal Purple",
        @"Gateron Black Ink", @"Gateron Red Ink V2", @"Holy Panda", @"Kailh Box Navy", @"NK Cream",
        @"Topre PBT", @"Turquoise Tealios"
    ];
}

static const unsigned long long kMaxZipBytes = 30ull * 1024 * 1024;
static const unsigned long long kMaxTreeBytes = 20ull * 1024 * 1024;
static const NSUInteger kMaxTreeEntries = 600;

/// NSError convention: callers may pass NULL for the error.
static void SetError(NSError **error, NSError *value) {
    if (error != NULL) {
        *error = value;
    }
}

static NSError *ImportError(NSString *message) {
    return [NSError errorWithDomain:KFPackErrorDomain
                               code:KFPackErrorMalformed
                           userInfo:@{NSLocalizedDescriptionKey : message}];
}

@interface KFPackLibrary ()
@property (nonatomic, readwrite, copy) NSArray<KFPackManifest *> *packs;
@property (nonatomic, readwrite, nullable) KFPackManifest *loadedPack;
@property (nonatomic, readwrite) double loadedRate;
@end

@implementation KFPackLibrary {
    NSURL *_bundledURL;
    dispatch_queue_t _decodeQueue;
    NSUInteger _generation; /* Drops a slow load superseded by a newer one. */
}

- (instancetype)initWithBundledURL:(nullable NSURL *)bundledURL userURL:(NSURL *)userURL {
    if ((self = [super init])) {
        _bundledURL = bundledURL;
        _userURL = userURL;
        _decodeQueue = dispatch_queue_create(
            "com.keyfeel.packs.decode",
            dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_UTILITY, 0));
        _packs = @[];
        [self reload];
    }
    return self;
}

- (NSArray<KFPackManifest *> *)scan:(nullable NSURL *)folder {
    if (folder == nil) {
        return @[];
    }
    NSMutableArray<KFPackManifest *> *found = [NSMutableArray array];
    for (NSURL *url in
         [NSFileManager.defaultManager contentsOfDirectoryAtURL:(NSURL *_Nonnull)folder
                                     includingPropertiesForKeys:nil
                                                        options:0
                                                          error:NULL]) {
        if (![url.pathExtension isEqualToString:@"kfpack"]) {
            continue;
        }
        NSError *error = nil;
        KFPackManifest *pack = [KFPackManifest manifestWithPackAtURL:url error:&error];
        if (pack != nil) {
            [found addObject:pack];
        } else {
            os_log_error(KFLogPacks(), "skipping %{public}@: %{public}@", url.lastPathComponent,
                         error.localizedDescription);
        }
    }
    return found;
}

- (void)reload {
    NSArray<NSString *> *order = SpecOrder();
    NSArray<KFPackManifest *> *bundled = [[self scan:_bundledURL]
        sortedArrayUsingComparator:^NSComparisonResult(KFPackManifest *a, KFPackManifest *b) {
            const NSUInteger ia = [order indexOfObject:a.name];
            const NSUInteger ib = [order indexOfObject:b.name];
            if (ia != ib) {
                return ia < ib ? NSOrderedAscending : NSOrderedDescending;
            }
            return [a.name localizedStandardCompare:b.name];
        }];
    NSArray<KFPackManifest *> *user = [[self scan:_userURL]
        sortedArrayUsingComparator:^NSComparisonResult(KFPackManifest *a, KFPackManifest *b) {
            return [a.name localizedStandardCompare:b.name];
        }];
    /* A user pack never shadows a bundled one with the same id. */
    NSMutableArray<KFPackManifest *> *all = [bundled mutableCopy];
    NSSet<NSString *> *ids = [NSSet setWithArray:[bundled valueForKey:@"identifier"]];
    for (KFPackManifest *pack in user) {
        if (![ids containsObject:pack.identifier]) {
            [all addObject:pack];
        }
    }
    self.packs = all;
    [NSNotificationCenter.defaultCenter postNotificationName:KFPackLibraryDidChangeNotification
                                                      object:self];
}

- (BOOL)isBundled:(KFPackManifest *)pack {
    return _bundledURL != nil && [KFPath(pack.packURL.URLByDeletingLastPathComponent)
                                     isEqualToString:KFPath(_bundledURL.URLByStandardizingPath)];
}

- (nullable KFPackManifest *)packWithIdentifier:(NSString *)identifier {
    for (KFPackManifest *pack in self.packs) {
        if ([pack.identifier isEqualToString:identifier]) {
            return pack;
        }
    }
    return nil;
}

- (nullable KFPackManifest *)resolvePack:(nullable NSString *)identifier {
    KFPackManifest *pack =
        identifier != nil ? [self packWithIdentifier:(NSString *_Nonnull)identifier] : nil;
    if (pack != nil) {
        return pack;
    }
    for (KFPackManifest *candidate in self.packs) {
        if ([candidate.switchType isEqualToString:@"linear"]) {
            return candidate; /* Spec default: the first finished linear pack. */
        }
    }
    return self.packs.firstObject;
}

#pragma mark - Loading

- (void)installPack:(kf_pack *)decoded manifest:(KFPackManifest *)pack rate:(double)rate {
    kf_engine_set_pack(decoded);
    self.loadedPack = pack;
    self.loadedRate = rate;
    [NSNotificationCenter.defaultCenter postNotificationName:KFPackLibraryDidChangeNotification
                                                      object:self];
}

- (void)loadPack:(KFPackManifest *)pack
            rate:(double)rate
      completion:(nullable void (^)(NSError *_Nullable error))completion {
    const NSUInteger generation = ++_generation;
    dispatch_async(_decodeQueue, ^{
        NSError *error = nil;
        kf_pack *decoded = [pack loadPackAtSampleRate:rate error:&error];
        dispatch_async(dispatch_get_main_queue(), ^{
            if (generation != self->_generation) {
                kf_pack_free(decoded); /* A newer load won. */
                return;
            }
            if (decoded != NULL) {
                [self installPack:decoded manifest:pack rate:rate];
            } else {
                os_log_error(KFLogPacks(), "%{public}@", error.localizedDescription);
            }
            if (completion != nil) {
                completion(error);
            }
        });
    });
}

- (BOOL)loadPackNow:(KFPackManifest *)pack rate:(double)rate error:(NSError **)error {
    ++_generation;
    kf_pack *decoded = [pack loadPackAtSampleRate:rate error:error];
    if (decoded == NULL) {
        return NO;
    }
    [self installPack:decoded manifest:pack rate:rate];
    return YES;
}

#pragma mark - Import and delete

/// Every entry under `root`: no symlinks or special files, bounded count and size.
static NSString *_Nullable CheckTree(NSURL *root) {
    NSDirectoryEnumerator<NSURL *> *walk = [NSFileManager.defaultManager enumeratorAtURL:root
                                                              includingPropertiesForKeys:nil
                                                                                 options:0
                                                                            errorHandler:nil];
    unsigned long long total = 0;
    NSUInteger count = 0;
    for (NSURL *url in walk) {
        struct stat st;
        if (lstat(KFPath(url).fileSystemRepresentation, &st) != 0) {
            return @"the pack could not be read";
        }
        if (S_ISLNK(st.st_mode)) {
            return [NSString stringWithFormat:@"%@ is a symbolic link", url.lastPathComponent];
        }
        if (!S_ISREG(st.st_mode) && !S_ISDIR(st.st_mode)) {
            return [NSString stringWithFormat:@"%@ is not a regular file", url.lastPathComponent];
        }
        total += (unsigned long long)st.st_size;
        if (++count > kMaxTreeEntries || total > kMaxTreeBytes) {
            return @"the pack is larger than 20 MB or 600 files";
        }
    }
    return nil;
}

/// The single .kfpack folder inside an extracted zip (or the folder itself).
static NSURL *_Nullable FindPack(NSURL *root) {
    if ([root.pathExtension isEqualToString:@"kfpack"]) {
        return root;
    }
    NSArray<NSURL *> *children = [NSFileManager.defaultManager contentsOfDirectoryAtURL:root
                                                             includingPropertiesForKeys:nil
                                                                                options:0
                                                                                  error:NULL];
    NSURL *found = nil;
    for (NSURL *child in children) {
        if ([child.pathExtension isEqualToString:@"kfpack"]) {
            if (found != nil) {
                return nil; /* Ambiguous: more than one pack. */
            }
            found = child;
        }
    }
    if (found == nil &&
        [NSFileManager.defaultManager
            fileExistsAtPath:[KFPath(root) stringByAppendingPathComponent:@"pack.json"]]) {
        found = root; /* Zipped contents without the folder. */
    }
    return found;
}

- (nullable KFPackManifest *)importWork:(NSURL *)source
                                  error:(NSError **)error
                                scratch:(NSURL *)scratch {
    NSFileManager *files = NSFileManager.defaultManager;
    NSURL *root = source;
    if ([source.pathExtension.lowercaseString isEqualToString:@"zip"]) {
        NSNumber *size = nil;
        [source getResourceValue:&size forKey:NSURLFileSizeKey error:NULL];
        if (size == nil || size.unsignedLongLongValue > kMaxZipBytes) {
            SetError(error, ImportError(@"The zip file is missing or larger than 30 MB."));
            return nil;
        }
        NSTask *ditto = [NSTask new];
        ditto.executableURL = [NSURL fileURLWithPath:@"/usr/bin/ditto"];
        ditto.arguments = @[ @"-x", @"-k", KFPath(source), KFPath(scratch) ];
        ditto.standardOutput = NSFileHandle.fileHandleWithNullDevice;
        ditto.standardError = NSFileHandle.fileHandleWithNullDevice;
        if (![ditto launchAndReturnError:error]) {
            return nil;
        }
        [ditto waitUntilExit];
        if (ditto.terminationStatus != 0) {
            SetError(error, ImportError(@"The zip file could not be extracted."));
            return nil;
        }
        root = scratch;
    }
    NSString *problem = CheckTree(root);
    if (problem != nil) {
        SetError(error, ImportError([NSString stringWithFormat:@"Not imported: %@.", problem]));
        return nil;
    }
    NSURL *packURL = FindPack(root);
    if (packURL == nil) {
        SetError(error, ImportError(@"No single .kfpack folder with a pack.json was found."));
        return nil;
    }
    KFPackManifest *pack = [KFPackManifest manifestWithPackAtURL:packURL error:error];
    if (pack == nil) {
        return nil;
    }
    for (KFPackManifest *existing in self.packs) {
        if ([existing.identifier isEqualToString:pack.identifier] && [self isBundled:existing]) {
            SetError(error, ImportError(@"A bundled pack already uses this id."));
            return nil;
        }
    }
    kf_pack *trial = [pack loadPackAtSampleRate:48000.0 error:error]; /* Validates the audio. */
    if (trial == NULL) {
        return nil;
    }
    kf_pack_free(trial);

    [files createDirectoryAtURL:_userURL withIntermediateDirectories:YES attributes:nil error:NULL];
    NSString *folder = [[pack.identifier stringByReplacingOccurrencesOfString:@"/" withString:@"-"]
        stringByAppendingPathExtension:@"kfpack"];
    NSURL *destination = [_userURL URLByAppendingPathComponent:folder isDirectory:YES];
    [files removeItemAtURL:destination error:NULL]; /* Replacing a user pack with a new version. */
    if (![files copyItemAtURL:packURL toURL:destination error:error]) {
        return nil;
    }
    return [KFPackManifest manifestWithPackAtURL:destination error:error];
}

- (void)importPackAtURL:(NSURL *)url
             completion:(void (^)(KFPackManifest *_Nullable, NSError *_Nullable))completion {
    NSURL *scratch =
        [NSURL fileURLWithPath:[KFCachesPath()
                                   stringByAppendingPathComponent:
                                       [@"Import-" stringByAppendingString:NSUUID.UUID.UUIDString]]
                   isDirectory:YES];
    dispatch_async(_decodeQueue, ^{
        [NSFileManager.defaultManager createDirectoryAtURL:scratch
                               withIntermediateDirectories:YES
                                                attributes:nil
                                                     error:NULL];
        NSError *error = nil;
        KFPackManifest *pack = [self importWork:url error:&error scratch:scratch];
        [NSFileManager.defaultManager removeItemAtURL:scratch error:NULL];
        dispatch_async(dispatch_get_main_queue(), ^{
            [self reload];
            completion(pack, pack == nil ? error : nil);
        });
    });
}

- (BOOL)deletePack:(KFPackManifest *)pack error:(NSError **)error {
    NSString *parent = KFPath(pack.packURL.URLByDeletingLastPathComponent);
    if ([self isBundled:pack] ||
        ![parent isEqualToString:KFPath(_userURL.URLByStandardizingPath)]) {
        if (error != NULL) {
            SetError(error, ImportError(@"Only imported packs can be deleted."));
        }
        return NO;
    }
    if (![NSFileManager.defaultManager removeItemAtURL:pack.packURL error:error]) {
        return NO;
    }
    [self reload];
    return YES;
}

- (NSArray<NSString *> *)attributions {
    NSMutableArray<NSString *> *lines = [NSMutableArray array];
    for (KFPackManifest *pack in self.packs) {
        if (pack.attribution.length > 0) {
            [lines addObject:[NSString stringWithFormat:@"%@: %@ (%@)", pack.name, pack.attribution,
                                                        pack.license]];
        }
    }
    return lines;
}

@end
