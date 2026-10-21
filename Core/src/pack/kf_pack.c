// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#include <kf/kf_pack.h>

#include <stdio.h>
#include <stdlib.h>

struct kf_pack {
    size_t decoded_bytes;
    /* TODO(M1): int16 sample buffers, per group/direction/tier take index, derived flags. */
};

kf_pack *kf_pack_load(const kf_pack_file *files, size_t count, double device_rate, char *err,
                      size_t errlen) {
    (void)files;
    (void)count;
    (void)device_rate;
    /*
     * TODO(M1): decode each file with ExtAudioFile into 16-bit mono at device_rate;
     * enforce KF_PACK_MAX_FILES, KF_PACK_MAX_DECODED_BYTES and KF_PACK_MAX_FILE_MS;
     * write every page once so playback never page-faults.
     */
    if (err != NULL && errlen > 0u) {
        (void)snprintf(err, errlen, "%s", "kf_pack_load: not implemented (M1)");
    }
    return NULL;
}

void kf_pack_free(kf_pack *pack) {
    free(pack); /* TODO(M1): free decoded buffers first. */
}

size_t kf_pack_decoded_bytes(const kf_pack *pack) {
    return pack != NULL ? pack->decoded_bytes : 0u;
}
