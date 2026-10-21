// SPDX-License-Identifier: LicenseRef-Proprietary
// Copyright (c) 2026 The Keyfeel Authors. All rights reserved.

/**
 * @file kf_keytap_internal.h
 * @brief Keytap module internals. Not part of the public API.
 */
#ifndef KF_KEYTAP_INTERNAL_H
#define KF_KEYTAP_INTERNAL_H

#include <kf/kf_types.h>

/**
 * Routes one classified key event: built-in key-downs go to the sensor thread
 * while it is LIVE; everything else plays at once on queue A (key-ups as the
 * release sound, key-downs at KF_DEFAULT_VELOCITY). Keytap thread; RT-safe.
 */
void kf_keytap_dispatch(const kf_key_event *event);

#endif /* KF_KEYTAP_INTERNAL_H */
