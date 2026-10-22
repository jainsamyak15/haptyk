// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#import "Packs/KFPackManifest.h"

#include <stdlib.h>
#include <sys/stat.h>

#include <kf/kf_config.h>
#include <kf/kf_types.h>

NSErrorDomain const KFPackErrorDomain = @"com.keyfeel.pack";

static const unsigned long long kMaxManifestBytes = 256u * 1024u;
/// 1.5 s of 24-bit 48 kHz mono is 216 KB; anything far larger is not a key sound.
static const unsigned long long kMaxAudioFileBytes = 2u * 1024u * 1024u;
static const NSUInteger kMaxPathLength = 255u;

static NSString *const kGroupNames[KF_GROUP_COUNT] = {@"alpha",     @"space",    @"enter",
                                                      @"backspace", @"modifier", @"tab"};
static NSString *const kTierNames[KF_TIER_COUNT] = {@"soft", @"medium", @"hard", @"slam"};

/// NSError convention: callers may pass NULL for the error.
static void SetError(NSError **error, NSError *value) {
    if (error != NULL) {
        *error = value;
    }
}

static NSError *PackError(KFPackError code, NSString *format, ...) NS_FORMAT_FUNCTION(2, 3);
static NSError *PackError(KFPackError code, NSString *format, ...) {
    va_list args;
    va_start(args, format);
    NSString *message = [[NSString alloc] initWithFormat:format arguments:args];
    va_end(args);
    return [NSError errorWithDomain:KFPackErrorDomain
                               code:code
                           userInfo:@{NSLocalizedDescriptionKey : message}];
}

static NSInteger IndexOf(NSString *const *names, NSInteger count, NSString *name) {
    for (NSInteger i = 0; i < count; ++i) {
        if ([names[i] isEqualToString:name]) {
            return i;
        }
    }
    return -1;
}

/// One audio file of the pack, resolved and checked.
@interface KFPackFileEntry : NSObject
@property (nonatomic) uint8_t group;
@property (nonatomic) uint8_t isUp;
@property (nonatomic) uint8_t tier;
@property (nonatomic) BOOL derived;
@property (nonatomic, copy) NSString *path;
@end

@implementation KFPackFileEntry
@end

@interface KFPackManifest ()
@property (nonatomic, readwrite) NSURL *packURL;
@property (nonatomic, readwrite, copy) NSString *identifier;
@property (nonatomic, readwrite, copy) NSString *name;
@property (nonatomic, readwrite, copy) NSString *switchType;
@property (nonatomic, readwrite, copy) NSString *author;
@property (nonatomic, readwrite, copy) NSString *license;
@property (nonatomic, readwrite, copy) NSString *version;
@property (nonatomic, readwrite, copy, nullable) NSString *sourceURL;
@property (nonatomic, readwrite, copy) NSString *attribution;
@property (nonatomic, readwrite) BOOL hasDerivedTiers;
@property (nonatomic, copy) NSArray<KFPackFileEntry *> *files;
@end

@implementation KFPackManifest

+ (NSArray<NSString *> *)switchTypes {
    return @[ @"linear", @"tactile", @"clicky", @"topre", @"buckling", @"other" ];
}

- (instancetype)initPrivate {
    return [super init];
}

- (NSUInteger)fileCount {
    return self.files.count;
}

#pragma mark - Reading

+ (nullable instancetype)manifestWithPackAtURL:(NSURL *)packURL error:(NSError **)error {
    NSError *failure = nil;
    KFPackManifest *manifest = [[self alloc] initPrivate];
    manifest.packURL = packURL.URLByStandardizingPath;
    if (![manifest readWithError:&failure]) {
        if (error != NULL) {
            SetError(error, failure);
        }
        return nil;
    }
    return manifest;
}

- (BOOL)readWithError:(NSError **)error {
    NSString *root = self.packURL.path;
    struct stat st;
    if (lstat(root.fileSystemRepresentation, &st) != 0 || !S_ISDIR(st.st_mode)) {
        SetError(error, PackError(KFPackErrorUnreadable, @"%@ is not a pack folder",
                                  root.lastPathComponent));
        return NO;
    }
    NSString *manifestPath = [root stringByAppendingPathComponent:@"pack.json"];
    if (lstat(manifestPath.fileSystemRepresentation, &st) != 0 || !S_ISREG(st.st_mode) ||
        (unsigned long long)st.st_size > kMaxManifestBytes) {
        SetError(error,
                 PackError(KFPackErrorUnreadable,
                           @"pack.json is missing, not a regular file, or larger than 256 KB"));
        return NO;
    }
    NSData *data = [NSData dataWithContentsOfFile:manifestPath options:0 error:NULL];
    id json =
        data != nil ? [NSJSONSerialization JSONObjectWithData:data options:0 error:NULL] : nil;
    if (![json isKindOfClass:NSDictionary.class]) {
        SetError(error, PackError(KFPackErrorMalformed, @"pack.json is not a JSON object"));
        return NO;
    }
    return [self readFields:json error:error] && [self readGroups:json error:error];
}

- (nullable NSString *)string:(NSDictionary *)json
                          key:(NSString *)key
                    maxLength:(NSUInteger)maxLength
                     required:(BOOL)required
                        error:(NSError **)error {
    id value = json[key];
    if ((value == nil || value == NSNull.null) && !required) {
        return nil;
    }
    if (![value isKindOfClass:NSString.class] || [value length] == 0 ||
        [value length] > maxLength) {
        SetError(error, PackError(KFPackErrorMalformed,
                                  @"pack.json: \"%@\" must be a string of 1 to %lu "
                                  @"characters",
                                  key, (unsigned long)maxLength));
        return nil;
    }
    return value;
}

- (BOOL)readFields:(NSDictionary *)json error:(NSError **)error {
    id format = json[@"format"];
    if (![format isKindOfClass:NSNumber.class] || [format integerValue] != 1) {
        SetError(error, PackError(KFPackErrorMalformed, @"pack.json: \"format\" must be 1"));
        return NO;
    }
    self.identifier = [self string:json key:@"id" maxLength:100 required:YES error:error];
    if (self.identifier == nil) {
        return NO;
    }
    NSCharacterSet *allowed =
        [NSCharacterSet characterSetWithCharactersInString:
                            @"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-"];
    if ([self.identifier rangeOfCharacterFromSet:allowed.invertedSet].location != NSNotFound ||
        ![self.identifier containsString:@"."]) {
        SetError(error,
                 PackError(KFPackErrorMalformed,
                           @"pack.json: \"id\" must be reverse-DNS, like com.example.my-pack"));
        return NO;
    }
    NSString *name = [self string:json key:@"name" maxLength:64 required:YES error:error];
    NSString *type =
        name ? [self string:json key:@"switch_type" maxLength:16 required:YES error:error] : nil;
    NSString *author =
        type ? [self string:json key:@"author" maxLength:128 required:YES error:error] : nil;
    NSString *license =
        author ? [self string:json key:@"license" maxLength:128 required:YES error:error] : nil;
    NSString *version =
        license ? [self string:json key:@"version" maxLength:32 required:YES error:error] : nil;
    if (version == nil) {
        return NO;
    }
    if (![KFPackManifest.switchTypes containsObject:type]) {
        SetError(error,
                 PackError(KFPackErrorMalformed, @"pack.json: \"switch_type\" must be one of %@",
                           [KFPackManifest.switchTypes componentsJoinedByString:@", "]));
        return NO;
    }
    NSError *optional = nil;
    NSString *source = [self string:json
                                key:@"source_url"
                          maxLength:2048
                           required:NO
                              error:&optional];
    if (optional != nil) {
        SetError(error, optional);
        return NO;
    }
    if (source != nil && ![source hasPrefix:@"https://"] && ![source hasPrefix:@"http://"]) {
        SetError(error,
                 PackError(KFPackErrorMalformed, @"pack.json: \"source_url\" must be a web URL"));
        return NO;
    }
    NSString *attribution = [self string:json
                                     key:@"attribution"
                               maxLength:1000
                                required:NO
                                   error:&optional];
    if (optional != nil && [json[@"attribution"] isEqual:@""]) {
        optional = nil; /* An empty attribution means none is required. */
    }
    if (optional != nil) {
        SetError(error, optional);
        return NO;
    }
    self.name = name;
    self.switchType = type;
    self.author = author;
    self.license = license;
    self.version = version;
    self.sourceURL = source;
    self.attribution = attribution ?: @"";
    return YES;
}

/// "alpha/slam" entries of the optional "derived" list, as a set of group * 4 + tier.
- (nullable NSSet<NSNumber *> *)readDerived:(NSDictionary *)json error:(NSError **)error {
    id derived = json[@"derived"];
    if (derived == nil || derived == NSNull.null) {
        return [NSSet set];
    }
    NSMutableSet<NSNumber *> *slots = [NSMutableSet set];
    if (![derived isKindOfClass:NSArray.class]) {
        SetError(error, PackError(KFPackErrorMalformed, @"pack.json: \"derived\" must be a list"));
        return nil;
    }
    for (id item in derived) {
        NSArray *parts =
            [item isKindOfClass:NSString.class] ? [item componentsSeparatedByString:@"/"] : nil;
        const NSInteger group =
            parts.count == 2 ? IndexOf(kGroupNames, KF_GROUP_COUNT, parts[0]) : -1;
        const NSInteger tier = parts.count == 2 ? IndexOf(kTierNames, KF_TIER_COUNT, parts[1]) : -1;
        if (group < 0 || tier < 0) {
            SetError(error, PackError(KFPackErrorMalformed,
                                      @"pack.json: \"derived\" entries look like \"alpha/slam\""));
            return nil;
        }
        [slots addObject:@(group * KF_TIER_COUNT + tier)];
    }
    return slots;
}

- (BOOL)readGroups:(NSDictionary *)json error:(NSError **)error {
    NSSet<NSNumber *> *derived = [self readDerived:json error:error];
    if (derived == nil) {
        return NO;
    }
    NSDictionary *groups = json[@"groups"];
    if (![groups isKindOfClass:NSDictionary.class]) {
        SetError(error,
                 PackError(KFPackErrorMalformed, @"pack.json: \"groups\" must be an object"));
        return NO;
    }
    NSMutableArray<KFPackFileEntry *> *files = [NSMutableArray array];
    for (NSString *groupName in [groups.allKeys sortedArrayUsingSelector:@selector(compare:)]) {
        const NSInteger group = IndexOf(kGroupNames, KF_GROUP_COUNT, groupName);
        NSDictionary *body = groups[groupName];
        if (group < 0 || ![body isKindOfClass:NSDictionary.class]) {
            SetError(error, PackError(KFPackErrorMalformed,
                                      @"pack.json: unknown group \"%@\" (use alpha, space, enter, "
                                      @"backspace, modifier or tab)",
                                      groupName));
            return NO;
        }
        for (NSString *key in body) {
            if (![key isEqualToString:@"down"] && ![key isEqualToString:@"up"]) {
                SetError(error,
                         PackError(KFPackErrorMalformed,
                                   @"pack.json: groups.%@ has unknown key \"%@\"", groupName, key));
                return NO;
            }
        }
        id down = body[@"down"];
        if (down != nil && ![down isKindOfClass:NSDictionary.class]) {
            SetError(error, PackError(KFPackErrorMalformed,
                                      @"pack.json: groups.%@.down must be an object", groupName));
            return NO;
        }
        for (NSString *tierName in [[down allKeys] sortedArrayUsingSelector:@selector(compare:)]) {
            const NSInteger tier = IndexOf(kTierNames, KF_TIER_COUNT, tierName);
            if (tier < 0) {
                SetError(error,
                         PackError(KFPackErrorMalformed,
                                   @"pack.json: groups.%@.down has unknown tier \"%@\" (use soft, "
                                   @"medium, hard or slam)",
                                   groupName, tierName));
                return NO;
            }
            NSString *field = [NSString stringWithFormat:@"groups.%@.down.%@", groupName, tierName];
            const BOOL isDerived = [derived containsObject:@(group * KF_TIER_COUNT + tier)];
            if (![self addFiles:down[tierName]
                          field:field
                          group:group
                             up:NO
                           tier:tier
                        derived:isDerived
                           into:files
                          error:error]) {
                return NO;
            }
            self.hasDerivedTiers = self.hasDerivedTiers || isDerived;
        }
        id up = body[@"up"];
        if (up != nil && ![self addFiles:up
                                   field:[NSString stringWithFormat:@"groups.%@.up", groupName]
                                   group:group
                                      up:YES
                                    tier:0
                                 derived:NO
                                    into:files
                                   error:error]) {
            return NO;
        }
    }
    BOOL alphaDown = NO;
    for (KFPackFileEntry *entry in files) {
        alphaDown = alphaDown || (entry.group == KF_GROUP_ALPHA && !entry.isUp);
    }
    if (!alphaDown) {
        SetError(error, PackError(KFPackErrorMalformed,
                                  @"pack.json: groups.alpha.down needs at least one sound"));
        return NO;
    }
    self.files = files;
    return YES;
}

- (BOOL)addFiles:(id)list
           field:(NSString *)field
           group:(NSInteger)group
              up:(BOOL)up
            tier:(NSInteger)tier
         derived:(BOOL)derived
            into:(NSMutableArray<KFPackFileEntry *> *)files
           error:(NSError **)error {
    if (![list isKindOfClass:NSArray.class] || [list count] == 0) {
        SetError(error, PackError(KFPackErrorMalformed,
                                  @"pack.json: %@ must be a non-empty list of paths", field));
        return NO;
    }
    NSUInteger index = 0;
    for (id item in list) {
        NSString *where = [NSString stringWithFormat:@"%@[%lu]", field, (unsigned long)index++];
        NSString *path = [self resolve:item field:where error:error];
        if (path == nil) {
            return NO;
        }
        if (files.count >= KF_PACK_MAX_FILES) {
            SetError(error, PackError(KFPackErrorLimit, @"pack.json lists more than %u files",
                                      (unsigned)KF_PACK_MAX_FILES));
            return NO;
        }
        KFPackFileEntry *entry = [KFPackFileEntry new];
        entry.group = (uint8_t)group;
        entry.isUp = up ? 1u : 0u;
        entry.tier = (uint8_t)tier;
        entry.derived = derived;
        entry.path = path;
        [files addObject:entry];
    }
    return YES;
}

/// An absolute path for `relative`, which must stay inside the pack without crossing a symlink
/// and name a regular WAV or CAF file of sane size.
- (nullable NSString *)resolve:(id)relative field:(NSString *)field error:(NSError **)error {
    if (![relative isKindOfClass:NSString.class] || [relative length] == 0 ||
        [relative length] > kMaxPathLength) {
        SetError(error,
                 PackError(KFPackErrorMalformed, @"pack.json: %@ must be a relative path", field));
        return nil;
    }
    NSString *text = relative;
    if ([text hasPrefix:@"/"] || [text containsString:@"\\"] ||
        [text rangeOfCharacterFromSet:NSCharacterSet.controlCharacterSet].location != NSNotFound) {
        SetError(error, PackError(KFPackErrorUnsafePath,
                                  @"pack.json: %@ must be a plain relative path", field));
        return nil;
    }
    NSString *extension = text.pathExtension.lowercaseString;
    if (![extension isEqualToString:@"wav"] && ![extension isEqualToString:@"caf"]) {
        SetError(error, PackError(KFPackErrorMalformed,
                                  @"pack.json: %@ must be a .wav or .caf file", field));
        return nil;
    }
    NSArray<NSString *> *parts = [text componentsSeparatedByString:@"/"];
    NSString *path = self.packURL.path;
    for (NSUInteger i = 0; i < parts.count; ++i) {
        NSString *part = parts[i];
        if (part.length == 0 || [part isEqualToString:@"."] || [part isEqualToString:@".."]) {
            SetError(error, PackError(KFPackErrorUnsafePath,
                                      @"pack.json: %@ leaves the pack folder", field));
            return nil;
        }
        path = [path stringByAppendingPathComponent:part];
        struct stat st;
        if (lstat(path.fileSystemRepresentation, &st) != 0) {
            SetError(error,
                     PackError(KFPackErrorMissingFile, @"%@ (%@) does not exist", text, field));
            return nil;
        }
        const BOOL last = i + 1 == parts.count;
        if (S_ISLNK(st.st_mode)) {
            SetError(error,
                     PackError(KFPackErrorUnsafePath, @"%@ (%@) is a symbolic link", text, field));
            return nil;
        }
        if (last && (!S_ISREG(st.st_mode) || (unsigned long long)st.st_size > kMaxAudioFileBytes)) {
            SetError(error,
                     PackError(KFPackErrorLimit, @"%@ (%@) is not a regular file of at most 2 MB",
                               text, field));
            return nil;
        }
        if (!last && !S_ISDIR(st.st_mode)) {
            SetError(error,
                     PackError(KFPackErrorMissingFile, @"%@ (%@) does not exist", text, field));
            return nil;
        }
    }
    return path;
}

#pragma mark - Decoding

- (nullable kf_pack *)loadPackAtSampleRate:(double)sampleRate error:(NSError **)error {
    return [self loadPackAtSampleRate:sampleRate boom:nil error:error];
}

- (nullable kf_pack *)loadPackAtSampleRate:(double)sampleRate
                                      boom:(nullable NSURL *)boomURL
                                     error:(NSError **)error {
    NSArray<KFPackFileEntry *> *entries = self.files;
    const NSUInteger count = entries.count + (boomURL != nil ? 1u : 0u);
    kf_pack_file *files = calloc(count, sizeof *files);
    if (files == NULL) {
        if (error != NULL) {
            SetError(error, PackError(KFPackErrorLimit, @"out of memory"));
        }
        return NULL;
    }
    kf_pack *pack = NULL;
    char message[256] = "";
    @autoreleasepool {
        for (NSUInteger i = 0; i < entries.count; ++i) {
            KFPackFileEntry *e = entries[i];
            files[i] =
                (kf_pack_file){e.group, e.isUp, e.tier, e.derived, e.path.fileSystemRepresentation};
        }
        if (boomURL != nil) {
            files[entries.count] = (kf_pack_file){KF_SOUND_BOOM, 0u, KF_TIER_SOFT, false,
                                                  boomURL.fileSystemRepresentation};
        }
        pack = kf_pack_load(files, count, sampleRate, message, sizeof message);
    }
    free(files);
    if (pack == NULL && error != NULL) {
        SetError(error, PackError(KFPackErrorDecode, @"%@: %s", self.name, message));
    }
    return pack;
}

@end
