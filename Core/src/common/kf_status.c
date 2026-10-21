// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

#include <kf/kf_types.h>

const char *kf_status_string(kf_status status) {
    switch (status) {
    case KF_OK:
        return "ok";
    case KF_ERR_INVALID_ARGUMENT:
        return "invalid argument";
    case KF_ERR_NO_MEMORY:
        return "out of memory";
    case KF_ERR_NO_PERMISSION:
        return "permission missing";
    case KF_ERR_NOT_FOUND:
        return "not found";
    case KF_ERR_DEVICE:
        return "device error";
    case KF_ERR_AUDIO:
        return "audio error";
    case KF_ERR_FORMAT:
        return "unsupported or malformed data";
    case KF_ERR_LIMIT:
        return "limit exceeded";
    case KF_ERR_STATE:
        return "invalid state";
    case KF_ERR_NOT_IMPLEMENTED:
        return "not implemented";
    }
    return "unknown status";
}
